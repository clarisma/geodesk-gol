// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "Updater.h"

#include <ranges>
#include <clarisma/io/FilePath.h>
#include <clarisma/zip/Zip.h>
#include <geodesk/feature/FeatureStore.h>
#include <geodesk/feature/ParentRelationIterator.h>
#include <geodesk/query/SimpleQuery.h>
#include <geodesk/query/TileIndexWalker.h>
#include <tile/tes/TesChecker.h>

#include "ChangeIngester.h"
#include "ChangeReader.h"
#include "tile/compiler/TileCompiler.h"

static const char READING_TASK_PREFIX[] = "Reading ";

UpdaterWorker::UpdaterWorker(Updater* updater) :
    updater_(updater),
    analyzer_(updater->model()),
    writer_(updater->model(), updater->tileCatalog())
{
}

void UpdaterWorker::processTask(UpdaterTask& task)
{
    switch(updater_->phase())
    {
    case Updater::Phase::SEARCH:
        analyze(task.tip());
        break;
    case Updater::Phase::PREPARE_UPDATE:
        prepareUpdate(task.tip());
        break;
    case Updater::Phase::APPLY_UPDATE:
        applyUpdate(task.entryNumber());
        break;
    }
}

void UpdaterWorker::analyze(Tip tip)
{
    Tile tile = updater_->tileCatalog().tileOfTip(tip);
    TilePtr pTile = TilePtr(analyzer_.model().store()->fetchTile(tip));
    analyzer_.analyze(tip, tile, pTile);
    updater_->taskCompleted();
}

void UpdaterWorker::prepareUpdate(Tip tip)
{
    DynamicBuffer buf(64 * 1024);
    writer_.write(updater_->model().getChangedTile(tip), &buf);

#ifndef NDEBUG
    /*
    TesChecker checker(tip, updater_->tileCatalog().tileOfTip(tip),
        reinterpret_cast<const uint8_t*>(buf.data()), buf.length());
    checker.dump(updater_->dumpPath());
    */
#endif

    updater_->postOutput(TesArchiveWriter::createTes(tip, buf.takeBytes()));
}

void UpdaterWorker::applyUpdate(int entryNumber)
{
    TileCompiler compiler(analyzer_.model().store());
    const TesArchiveEntry& entry = updater_->tesEntry(entryNumber);
    Tip tip = entry.tip;
    LOGS << "Updating Tile " << tip;
    compiler.modifyTile(tip, updater_->tileCatalog().tileOfTip(tip));

    ByteBlock tesBlock = Zip::inflate(updater_->tesData(entryNumber), entry.size, entry.sizeUncompressed);
    Zip::verifyChecksum(tesBlock, entry.checksum);
    compiler.addChanges(tesBlock);
    // compiler.addChanges(updater_->tesEntry(entryNumber), updater_->tesData(entryNumber));
    ByteBlock block = compiler.compile();
    uint32_t size = static_cast<uint32_t>(block.size());
    updater_->postOutput({tip, std::move(block.take()), size});
}


Updater::Updater(FeatureStore* store, UpdateSettings& settings) :
    TaskEngine(settings.threadCount()),
    model_(store, settings),
    tileCatalog_(store),
    workCompleted_(0),
    updateFileName_(Strings::combine(
        FilePath::withoutExtension(store->fileName()), "-update.tes")),
    workPerUnit_(0),
    phase_(Phase::SEARCH),
    phaseCompleted_(0),
    targetRevision_(0)
{
    memcpy(displayBuffer_[0], READING_TASK_PREFIX, sizeof(READING_TASK_PREFIX) - 1);
    memcpy(displayBuffer_[1], READING_TASK_PREFIX, sizeof(READING_TASK_PREFIX) - 1);
}


void Updater::startPhase(Phase phase, int taskCount, double workPerUnit)
{
    phase_ = phase;
    tasksRemaining_ = taskCount;
    workPerUnit_ = workPerUnit;
}


void Updater::completed(double work)
{
    int percentage = static_cast<int>(workCompleted_.fetch_add(work) + work);
    Console::get()->setProgress(percentage);
}


void Updater::taskCompleted()
{
    completed(workPerUnit_);
    if(--tasksRemaining_ == 0)
    {
        LOGS << "Phase completed, releasing semaphore.";
        phaseCompleted_.release();
    }
}

void Updater::processTask(TileData& task)
{
    if(phase_ == Phase::PREPARE_UPDATE)
    {
        LOGS << "Writing tes for " << task.tip() << ": "
            << task.sizeOriginal() << " bytes ("
            << task.sizeCompressed() << " bytes compressed)";
        archiveWriter_.write(std::move(task));
    }
    else
    {
        assert(phase_ == Phase::APPLY_UPDATE);
    }
    taskCompleted();
}

void Updater::postProcess()
{
    int64_t unchangedTags = 0;
    for(auto& worker: workContexts())
    {
        unchangedTags += worker.unchangedTags();
    }
    LOGS << unchangedTags << " unchanged tags";
}

void Updater::readChangeFiles(std::span<const char*> files)
{
    double halfWorkPerFile = workReading_ / static_cast<double>(files.size()) / 2;

    Console::get()->start("Reading...");
    for(const char* changeFileName: files)
    {
        // Console::log("Reading %s...", changeFileName);
        LOGS << "Reading " << changeFileName;
        bool isCompressed = strcmp(FilePath::extension(changeFileName), ".gz") == 0;
        // TODO: string_view
        ByteBlock osc = File::readAll(changeFileName);
        LOGS << "  Read file.";
        // assert(_CrtCheckMemory());
        if(isCompressed)
        {
            osc = Zip::inflateGzip(osc);
            LOGS << "  Inflated contents.";
        }
        completed(halfWorkPerFile);
        osc.data()[osc.size()-1] = '\0';
        // force null-terminator, possibly overwriting > of final closing tag
        ChangeReader reader(model_, reinterpret_cast<char*>(osc.data()));
        //assert(_CrtCheckMemory());
        reader.read();
        completed(halfWorkPerFile);
        //assert(_CrtCheckMemory());
    }
    LOGS << "All files read.";
}

// void Updater::update(const char* changeFileName)
void Updater::update(std::string_view url, std::span<const char*> files)
{
    FeatureStore* store = model_.store();
    std::string shortName(FilePath::name(store->fileName()));
    if(!store->hasWaynodeIds())
    {
        throw IOException("Cannot update %s since it does not store waynode IDs "
            "(Must be built with option -w)", shortName.c_str());
    }

    LOGS << "Current revision: " << store->revision();

    ConsoleWriter out;
    out << "Updating "
        << Console::FAINT_LIGHT_BLUE << shortName << Console::DEFAULT;

    // TODO: take default URL from GOL
    if (!url.empty())
    {
        out << " via "
            << Console::FAINT_LIGHT_BLUE << url << Console::DEFAULT;
    }
    else
    {
        out << " from "
            << Console::FAINT_LIGHT_BLUE << files.size() << Console::DEFAULT
            << (files.size() == 1 ? " file" : " files");
    }
    out.flush();

    calculateWork(24 * 60 * 60);
        // Use 1 day if we don't know the exact timespan (TODO)

    dumpPath_ = FilePath::withoutExtension(store->fileName());
    dumpPath_ += "-tes";

    //assert(_CrtCheckMemory());
    start();

    if (!url.empty())
    {
        Console::get()->start("Checking for updates...");
        ChangeIngester ingester(*this);
        ingester.download(url);
        auto status = ingester.status();
        auto error = ingester.error();
        if(status == ChangeIngester::Status::NO_UPDATES)
        {
            if (!error.empty())
            {
                throw std::runtime_error(std::string(error));
            }
            Console::end().success() << "No updates available\n";
            return;
        }
    }
    else
    {
        readChangeFiles(files);
    }
#ifndef NDEBUG
    model_.dumpChangedRelationCount();
#endif

    Console::get()->setTask("Analyzing...");
    LOGS << "Preparing for analysis...";
    model_.prepareNodes();
    model_.prepareWays();
    //assert(_CrtCheckMemory());

    LOGS << "Starting analysis...";

    startPhase(Phase::SEARCH, store->tileCount(), workAnalyzing_ / store->tileCount());
    TileIndexWalker tiw(store->tileIndex(), store->zoomLevels(), Box::ofWorld(), nullptr);
    do
    {
        // LOGS << "Submitting " << tiw.currentTip();
        postWork(UpdaterTask(tiw.currentTip()));
    }
    while(tiw.next());
    awaitPhaseCompletion();
    for (UpdaterWorker& worker: workContexts())
    {
        worker.applyActions();
    }

#ifndef NDEBUG
    model_.dump();
    model_.checkMissing();
#endif
    processChanges();
    LOGS << model_.changedTiles().size() << " tiles changed.";

    prepareUpdate();
    applyUpdate();

    end();
    //assert(_CrtCheckMemory());

    Console::end().success() << "Updated " << model_.changedTiles().size() << " tiles.\n";
}


void Updater::prepareUpdate()
{
    LOGS << "Preparing update...";
    Console::get()->setTask("Preparing update...");

    TesChecker::createFolders(dumpPath_,
        model_.changedTiles() | std::views::keys);

    FeatureStore* store = model_.store();
    int changedTileCount = static_cast<int>(model_.changedTiles().size());
    startPhase(Phase::PREPARE_UPDATE, changedTileCount, 0);
        // TODO: workPerUnit
    archiveWriter_.open(updateFileName_.c_str(), store->guid(),
        targetRevision_, targetTimestamp_, changedTileCount);
    for(const auto& [tip,changedTile] : model_.changedTiles())
    {
        postWork(UpdaterTask(tip));
    }
    awaitPhaseCompletion();
    archiveWriter_.close();
    LOGS << "Prepared update.";
}


void Updater::applyUpdate()
{
    LOGS << "Updating tiles...";
    Console::get()->setTask("Updating tiles...");
    FeatureStore* store = model_.store();

    tesArchive_.open(updateFileName_.c_str());
    int changedTileCount = static_cast<int>(tesArchive_.header().entryCount);
    tesOffsets_.reset(new uint64_t[changedTileCount]);
    assert(changedTileCount == model_.changedTiles().size());
    startPhase(Phase::APPLY_UPDATE, changedTileCount,
        workApplying_ / changedTileCount);

    uint64_t currentOfs = sizeof(TesArchiveHeader) + sizeof(TesArchiveEntry) * changedTileCount;
    for(int i=0; i<changedTileCount; i++)
    {
        const TesArchiveEntry& entry = tesArchive_[i];
        tesOffsets_[i] = currentOfs;
        currentOfs += entry.size;
        postWork(UpdaterTask(i));
    }
    awaitPhaseCompletion();
    LOGS << "Updated " << changedTileCount << " tiles.";
}


void Updater::calculateWork(int timespanInSeconds)
{
    double parallelismFactor = 1.0 / (static_cast<double>(threadCount()) * 0.75);

    // The ratio of analyzing/updating tiles vs. reading and
    // parsing the .osc data declines logarithmically as the
    // update covers a longer timespan, since we can amortize
    // the relatively high per-tile cost across more changes

    double t = static_cast<double>(timespanInSeconds) / 3600.0;

    // Logistic parameters
    const double U = 270.0;  // Upper plateau
    const double L =  32.0;  // Lower plateau
    const double M = 7.0;   // Midpoint (in hours)
    const double S =  2.1;   // Steepness

    // Logistic-style decay
    double x = std::pow(t / M, S);
    double ratio = L + (U - L) / (1.0 + x);

    if(/* indexing */ false)
    {

    }
    else
    {
        workReading_ = 50.0 / (1 + ratio * parallelismFactor);
        workAnalyzing_ = 50;
        workApplying_ = 50.0 - workReading_;
    }
}


void Updater::printRevision(ConsoleWriter& out, const char* leader, uint32_t revision,
    DateTime timestamp, DateTime now)
{
    constexpr int REVISION_MAX_DIGITS = 8;
    char buf[64];
    out << leader << Console::FAINT_LIGHT_BLUE;
    memset(buf, ' ', REVISION_MAX_DIGITS);
    Format::unsignedIntegerReverse(revision, buf+REVISION_MAX_DIGITS);
    // TODO: may underflow buffer if >8 digits!
    out.writeBytes(buf, REVISION_MAX_DIGITS);
    out << Console::DEFAULT << " • " << timestamp << " (";
    Format::timeAgo(buf, (now - timestamp) / 1000);
    out << buf << ")\n";
}


void Updater::beginUpdate(
    uint32_t fromRevision, DateTime fromTimestamp,
    uint32_t toRevision, DateTime toTimestamp)
{
    changeFileCount_ = toRevision - fromRevision;
    assert(changeFileCount_ > 0);
    calculateWork((toTimestamp - fromTimestamp) / 1000);

    DateTime now = DateTime::now();
    ConsoleWriter out;
    out.blank();        // ensures that progress bar is fully overwritten
    printRevision(out, "    from ", fromRevision, fromTimestamp, now);
    printRevision(out, "      to ", toRevision, toTimestamp, now);
    out.flush();
    setReadingTask(fromRevision+1);
    // TODO: this causes the task display to print twice, can we add
    //  a method to Console that sets the task without printing it?
    //  (We would then call this method *before* writing to the Console)
}

void Updater::setReadingTask(uint32_t revision)
{
    char* buf = displayBuffer_[useAltDisplay_];
    char* p = Format::integer(buf + sizeof(READING_TASK_PREFIX-1), revision);
    memcpy(p, "...", 4);
    Console::get()->setTask(buf);
    useAltDisplay_ = !useAltDisplay_;
}

void Updater::reportFileRead(size_t uncompressedSize)
{
    // TODO: Use higher of count or progress towards buffer size
    completed(workReading_ / static_cast<double>(changeFileCount_));
}
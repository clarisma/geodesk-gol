# Setting up the basic test for GOL UPDATE

I have two repositories, `geodesk-gol` and `test.geodesk.com`. Please create a script named `prepare_osc.py` that will live in `geodesk-gol/test/tools`. This script takes a single parameter, hereafter referred to as `$NAME` (e.g. `basic/day`).

The script performs the following actions (Use paths relative to the scripts location):

- Use $TEST_SOURCE = geodesk-gol/test/update/data/${NAME}

- Use $TEST_TARGET = test.geodesk.com/update/${NAME}

- Find all files with the pattern `*.osc` in {$TEST_SOURCE}

- Each file has a numeric name (e.g. `1234.osc`). For each file, extract the number from the name and create a gzipped file with the contents of this file, with a path in the form `${TEST_TARGET}/000/001/234.osc.gz`. Create directories if needed.

  - Create a file named `${TEST_TARGET}/state.txt` with the following content (do not indent lines):

      timestamp=${TIMESTAMP}
      sequenceNumber=${HIGHEST_NUMBER}

  where ${TIMESTAMP} is the current time (formatted like "2026-10-17T20\:20\:51Z"; colons must be escaped since this is a Java properties file) and ${HIGHEST_NUMBER} is the highest number of the `.osc` files processed in the previous step

Here's an example of the state.txt file contents:

timestamp=2026-10-17T20\:20\:51Z
sequenceNumber=4941










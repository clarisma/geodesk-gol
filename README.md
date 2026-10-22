<img src="https://docs.geodesk.com/img/github-header.png">

# Geo-Object Librarian (GOL)

- Build and manage Geo-Object Libraries (GeoDesk's compact database format for OpenStreetMap features).

- Run [GOQL](http://docs.geodesk.com/goql) queries and export results in multiple formats.

## Setup

`gol` is a single self-contained executable. Simply [download](https://www.geodesk.com/download) and unzip.

## Usage

First, build a GOL from OpenStreetMap data:

```
gol build france france-latest.osm.pbf  
# produces france.gol
```

Extract the boundary of Paris as GeoJSON:

```
gol query france a[boundary=administrative][admin_level=6][name=Paris] > paris.geojson
```

Display the museums of Paris on a map:

```
gol map france red: na[tourism=museum] -a paris.geojson
```

Get the names and phone numbers of all restaurants in France:

```
gol query france na[amenity=restaurant] -f csv -k name,phone
```

[Full documentation](https://docs.geodesk.com/gol)

## Building from source

See [doc/build.md](doc/build.md)

## Toolkits

Build fast and powerful geospatial applications with the GeoDesk OpenStreetMap Toolkits for [Java](https://github.com/clarisma/geodesk), [Python](https://github.com/clarisma/geodesk-py) and [C++](https://github.com/clarisma/libgeodesk).

---

*OpenStreetMap is a trademark of the OpenStreetMap Foundation, and is used with their permission. GeoDesk is not endorsed by or affiliated with the OpenStreetMap Foundation.*

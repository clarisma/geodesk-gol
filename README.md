<img src="https://docs.geodesk.com/img/github-header.png">

# Geo-Object Librarian (GOL) 2.0

- Build and manage Geo-Object Libraries (GeoDesk's compact database format for OpenStreetMap features).

- Run [GOQL](http://docs2.geodesk.com/goql) queries and export results in multiple formats.

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

📖 [Full documentation](https://docs2.geodesk.com/gol)

## Toolkits

Build fast and powerful geospatial applications with the GeoDesk OpenStreetMap Toolkits for [Java](https://github.com/clarisma/geodesk), [Python](https://github.com/clarisma/geodesk-py) and [C++](https://github.com/clarisma/libgeodesk).

> [!IMPORTANT]
> The GOL file format has changed in Version 2.0. Files created with GOL 2.0 are not compatible with Toolkits from Version 1.x, and vice versa.
> 
> To build GOLs for Version 1.x, use the legacy [Java-based GOL Tool](https://github.com/clarisma/gol-tool). 

# Graph Report - Hopper Test Stand  (2026-10-01)

## Corpus Check
- 18 files · ~12,690 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 65 nodes · 115 edges · 6 communities detected
- Extraction: 62% EXTRACTED · 38% INFERRED · 0% AMBIGUOUS · INFERRED: 44 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Community 0|Community 0]]
- [[_COMMUNITY_Community 1|Community 1]]
- [[_COMMUNITY_Community 2|Community 2]]
- [[_COMMUNITY_Community 3|Community 3]]
- [[_COMMUNITY_Community 4|Community 4]]
- [[_COMMUNITY_Community 5|Community 5]]

## God Nodes (most connected - your core abstractions)
1. `buildDataJson()` - 11 edges
2. `calLock()` - 10 edges
3. `calUnlock()` - 10 edges
4. `sensorTare()` - 8 edges
5. `setup()` - 7 edges
6. `setRelay()` - 6 edges
7. `sensorResetFilter()` - 6 edges
8. `updateSample()` - 6 edges
9. `setUnitLabel()` - 5 edges
10. `loop()` - 5 edges

## Surprising Connections (you probably didn't know these)
- `sensorResetFilter()` --calls--> `handleSim()`  [INFERRED]
  sensor.cpp → web_server.cpp
- `sensorTare()` --calls--> `handleTare()`  [INFERRED]
  sensor.cpp → web_server.cpp
- `calLock()` --calls--> `sensorRawAverage()`  [INFERRED]
  calibration.cpp → sensor.cpp
- `calLock()` --calls--> `sensorTare()`  [INFERRED]
  calibration.cpp → sensor.cpp
- `calLock()` --calls--> `updateSample()`  [INFERRED]
  calibration.cpp → sensor.cpp

## Communities

### Community 0 - "Community 0"
Cohesion: 0.21
Nodes (10): loop(), setup(), sensorInit(), temperatureC(), temperatureInit(), temperatureTick(), temperatureValid(), handleCapture() (+2 more)

### Community 1 - "Community 1"
Cohesion: 0.33
Nodes (9): medianOf(), readAverageRaw(), sensorRawAverage(), sensorResetFilter(), sensorTare(), sensorTaskFn(), simulatedRaw(), updateSample() (+1 more)

### Community 2 - "Community 2"
Cohesion: 0.53
Nodes (8): calLock(), calUnlock(), computeCalibration(), loadCalibration(), saveCalibration(), setUnitLabel(), handleCalibrate(), handleSetCal()

### Community 3 - "Community 3"
Cohesion: 0.36
Nodes (7): currentIP(), startFallbackAP(), startWiFi(), wifiIsAP(), wifiRSSI(), sensorOnline(), buildDataJson()

### Community 4 - "Community 4"
Cohesion: 0.54
Nodes (7): applyRelay(), cycleRelay(), pulseRelay(), relayInit(), relayTick(), setRelay(), handleRelay()

### Community 5 - "Community 5"
Cohesion: 0.29
Nodes (4): handleData(), handleSaveCal(), handleSim(), handleTare()

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `buildDataJson()` connect `Community 3` to `Community 0`, `Community 2`, `Community 5`?**
  _High betweenness centrality (0.172) - this node is a cross-community bridge._
- **Why does `setup()` connect `Community 0` to `Community 2`, `Community 3`, `Community 4`?**
  _High betweenness centrality (0.116) - this node is a cross-community bridge._
- **Why does `handleSerial()` connect `Community 1` to `Community 0`, `Community 4`?**
  _High betweenness centrality (0.075) - this node is a cross-community bridge._
- **Are the 8 inferred relationships involving `buildDataJson()` (e.g. with `calLock()` and `calUnlock()`) actually correct?**
  _`buildDataJson()` has 8 INFERRED edges - model-reasoned connections that need verification._
- **Are the 5 inferred relationships involving `calLock()` (e.g. with `sensorRawAverage()` and `sensorTare()`) actually correct?**
  _`calLock()` has 5 INFERRED edges - model-reasoned connections that need verification._
- **Are the 5 inferred relationships involving `calUnlock()` (e.g. with `sensorRawAverage()` and `sensorTare()`) actually correct?**
  _`calUnlock()` has 5 INFERRED edges - model-reasoned connections that need verification._
- **Are the 4 inferred relationships involving `sensorTare()` (e.g. with `calLock()` and `calUnlock()`) actually correct?**
  _`sensorTare()` has 4 INFERRED edges - model-reasoned connections that need verification._
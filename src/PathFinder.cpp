#include "../include/PathFinder.h"
#include <cmath>
#include <queue>
#include <unordered_map>
#include <algorithm>
#include <chrono>
#include <limits>

double PathFinder::calculateEuclideanDistance(const Landmark& a, const Landmark& b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return std::hypot(dx, dy);
}

RouteResult PathFinder::findRouteAStar(const Graph& graph,
                                      const std::string& startId,
                                      const std::string& targetId,
                                      CostMetric metric) {
    auto startTime = std::chrono::high_resolution_clock::now();
    RouteResult result;
    result.startId = startId;
    result.targetId = targetId;
    result.algorithmUsed = "A* (A-Star)";

    if (!graph.hasLandmark(startId) || !graph.hasLandmark(targetId)) {
        result.success = false;
        result.statusMessage = "Error: Start or Target landmark ID not found in graph.";
        return result;
    }

    if (startId == targetId) {
        result.success = true;
        result.pathNodes.push_back(startId);
        result.totalDistanceMeters = 0.0;
        result.totalTimeSeconds = 0.0;
        result.nodesExplored = 1;
        result.statusMessage = "Start and target are identical.";
        return result;
    }

    const Landmark& targetLm = graph.getLandmark(targetId);

    // Heuristic estimate: distance or time (assuming max speed 60 km/h = 16.67 m/s)
    auto heuristic = [&](const std::string& nodeId) -> double {
        const Landmark& lm = graph.getLandmark(nodeId);
        double dist = calculateEuclideanDistance(lm, targetLm);
        if (metric == CostMetric::FASTEST_TIME) {
            // Admissible heuristic for time: dist / max_speed_allowed (16.67 m/s)
            return dist / 16.67;
        }
        return dist;
    };

    // Priority queue stores pair<fScore, nodeId>
    typedef std::pair<double, std::string> PQElement;
    std::priority_queue<PQElement, std::vector<PQElement>, std::greater<PQElement>> openSet;

    std::unordered_map<std::string, double> gScore;
    std::unordered_map<std::string, double> fScore;
    std::unordered_map<std::string, std::string> cameFromNode;
    std::unordered_map<std::string, std::string> cameFromRoad;
    std::unordered_map<std::string, double> roadDistAccum;
    std::unordered_map<std::string, double> roadTimeAccum;

    for (const auto& pair : graph.getLandmarks()) {
        gScore[pair.first] = std::numeric_limits<double>::infinity();
        fScore[pair.first] = std::numeric_limits<double>::infinity();
    }

    gScore[startId] = 0.0;
    fScore[startId] = heuristic(startId);
    openSet.push({fScore[startId], startId});

    int exploredCount = 0;
    bool found = false;

    while (!openSet.empty()) {
        auto current = openSet.top();
        openSet.pop();

        double currentF = current.first;
        std::string currentId = current.second;

        // If duplicate entry with worse cost, skip
        if (currentF > fScore[currentId]) continue;

        exploredCount++;

        if (currentId == targetId) {
            found = true;
            break;
        }

        for (const auto& edge : graph.getNeighbors(currentId)) {
            if (edge.isBlocked) continue; // Skip blocked roads / construction

            double edgeCost = (metric == CostMetric::FASTEST_TIME)
                                ? edge.travelTimeSeconds()
                                : edge.distance;

            double tentativeG = gScore[currentId] + edgeCost;
            if (tentativeG < gScore[edge.to]) {
                cameFromNode[edge.to] = currentId;
                cameFromRoad[edge.to] = edge.id;
                roadDistAccum[edge.to] = edge.distance;
                roadTimeAccum[edge.to] = edge.travelTimeSeconds();

                gScore[edge.to] = tentativeG;
                fScore[edge.to] = tentativeG + heuristic(edge.to);
                openSet.push({fScore[edge.to], edge.to});
            }
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
    result.nodesExplored = exploredCount;

    if (!found) {
        result.success = false;
        result.statusMessage = "No reachable path found (possible active road closures).";
        return result;
    }

    // Reconstruct path
    std::vector<std::string> path;
    std::vector<std::string> roads;
    std::string curr = targetId;
    double totalDist = 0.0;
    double totalTime = 0.0;

    while (curr != startId) {
        path.push_back(curr);
        roads.push_back(cameFromRoad[curr]);
        totalDist += roadDistAccum[curr];
        totalTime += roadTimeAccum[curr];
        curr = cameFromNode[curr];
    }
    path.push_back(startId);

    std::reverse(path.begin(), path.end());
    std::reverse(roads.begin(), roads.end());

    result.success = true;
    result.pathNodes = path;
    result.pathRoads = roads;
    result.totalDistanceMeters = totalDist;
    result.totalTimeSeconds = totalTime;
    result.statusMessage = "Optimal route successfully computed via A*.";
    return result;
}

RouteResult PathFinder::findRouteDijkstra(const Graph& graph,
                                          const std::string& startId,
                                          const std::string& targetId,
                                          CostMetric metric) {
    auto startTime = std::chrono::high_resolution_clock::now();
    RouteResult result;
    result.startId = startId;
    result.targetId = targetId;
    result.algorithmUsed = "Dijkstra";

    if (!graph.hasLandmark(startId) || !graph.hasLandmark(targetId)) {
        result.success = false;
        result.statusMessage = "Error: Start or Target landmark ID not found in graph.";
        return result;
    }

    if (startId == targetId) {
        result.success = true;
        result.pathNodes.push_back(startId);
        result.totalDistanceMeters = 0.0;
        result.totalTimeSeconds = 0.0;
        result.nodesExplored = 1;
        result.statusMessage = "Start and target are identical.";
        return result;
    }

    typedef std::pair<double, std::string> PQElement;
    std::priority_queue<PQElement, std::vector<PQElement>, std::greater<PQElement>> pq;

    std::unordered_map<std::string, double> dist;
    std::unordered_map<std::string, std::string> cameFromNode;
    std::unordered_map<std::string, std::string> cameFromRoad;
    std::unordered_map<std::string, double> roadDistAccum;
    std::unordered_map<std::string, double> roadTimeAccum;

    for (const auto& pair : graph.getLandmarks()) {
        dist[pair.first] = std::numeric_limits<double>::infinity();
    }

    dist[startId] = 0.0;
    pq.push({0.0, startId});

    int exploredCount = 0;
    bool found = false;

    while (!pq.empty()) {
        auto current = pq.top();
        pq.pop();

        double currentCost = current.first;
        std::string currentId = current.second;

        if (currentCost > dist[currentId]) continue;

        exploredCount++;

        if (currentId == targetId) {
            found = true;
            break;
        }

        for (const auto& edge : graph.getNeighbors(currentId)) {
            if (edge.isBlocked) continue; // Skip blocked roads

            double edgeCost = (metric == CostMetric::FASTEST_TIME)
                                ? edge.travelTimeSeconds()
                                : edge.distance;

            double nextCost = currentCost + edgeCost;
            if (nextCost < dist[edge.to]) {
                dist[edge.to] = nextCost;
                cameFromNode[edge.to] = currentId;
                cameFromRoad[edge.to] = edge.id;
                roadDistAccum[edge.to] = edge.distance;
                roadTimeAccum[edge.to] = edge.travelTimeSeconds();
                pq.push({nextCost, edge.to});
            }
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
    result.nodesExplored = exploredCount;

    if (!found) {
        result.success = false;
        result.statusMessage = "No reachable path found (possible active road closures).";
        return result;
    }

    // Backtrack path
    std::vector<std::string> path;
    std::vector<std::string> roads;
    std::string curr = targetId;
    double totalDist = 0.0;
    double totalTime = 0.0;

    while (curr != startId) {
        path.push_back(curr);
        roads.push_back(cameFromRoad[curr]);
        totalDist += roadDistAccum[curr];
        totalTime += roadTimeAccum[curr];
        curr = cameFromNode[curr];
    }
    path.push_back(startId);

    std::reverse(path.begin(), path.end());
    std::reverse(roads.begin(), roads.end());

    result.success = true;
    result.pathNodes = path;
    result.pathRoads = roads;
    result.totalDistanceMeters = totalDist;
    result.totalTimeSeconds = totalTime;
    result.statusMessage = "Optimal route successfully computed via Dijkstra.";
    return result;
}

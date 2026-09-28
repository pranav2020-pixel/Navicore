#include "../include/Graph.h"
#include "../include/PathFinder.h"
#include "../include/NavigationEngine.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <chrono>

void printBanner() {
    std::cout << "========================================================\n";
    std::cout << "  CAMPUS & SMART CITY DUAL NAVIGATION SYSTEM (C++ DSA) \n";
    std::cout << "  Core Algorithms: Dijkstra & A* with Heuristics        \n";
    std::cout << "  Dynamic Blockage & Real-time Detour Rerouting Engine  \n";
    std::cout << "========================================================\n";
}

bool loadMap(Graph& g, const std::string& mapName, const std::string& basePath = "data/") {
    std::string nodesPath = basePath + (mapName == "city" ? "city_nodes.csv" : "campus_nodes.csv");
    std::string edgesPath = basePath + (mapName == "city" ? "city_edges.csv" : "campus_edges.csv");

    // Fallback check if running from inside projectsite directory or parent
    std::ifstream testFile(nodesPath);
    if (!testFile.is_open()) {
        nodesPath = "projectsite/" + nodesPath;
        edgesPath = "projectsite/" + edgesPath;
    } else {
        testFile.close();
    }

    return g.loadFromCSV(nodesPath, edgesPath);
}

void printRouteSummary(const Graph& graph, const RouteResult& res, const std::vector<TurnInstruction>& directions) {
    std::cout << "\n---------------- ROUTE COMPUTATION SUMMARY ----------------\n";
    std::cout << "Status         : " << (res.success ? "SUCCESS" : "FAILED") << "\n";
    std::cout << "Algorithm      : " << res.algorithmUsed << "\n";
    std::cout << "Origin         : " << graph.getLandmark(res.startId).name << " (" << res.startId << ")\n";
    std::cout << "Destination    : " << graph.getLandmark(res.targetId).name << " (" << res.targetId << ")\n";
    std::cout << "Total Distance : " << std::fixed << std::setprecision(1) << res.totalDistanceMeters << " meters\n";
    std::cout << "Estimated Time : " << std::fixed << std::setprecision(1) << (res.totalTimeSeconds / 60.0) << " mins ("
              << res.totalTimeSeconds << " s)\n";
    std::cout << "Nodes Explored : " << res.nodesExplored << "\n";
    std::cout << "Compute Time   : " << res.executionMicroseconds << " microseconds (us)\n";
    std::cout << "Path Sequence  : ";
    for (size_t i = 0; i < res.pathNodes.size(); ++i) {
        std::cout << res.pathNodes[i] << (i + 1 < res.pathNodes.size() ? " -> " : "\n");
    }
    std::cout << "\n--- Turn-by-Turn Guidance ---\n";
    for (const auto& d : directions) {
        std::cout << " [" << d.stepNumber << "] " << std::setw(18) << std::left << d.action
                  << " : " << d.detailedText;
        if (d.segmentDistanceMeters > 0) {
            std::cout << " (" << std::fixed << std::setprecision(0) << d.segmentDistanceMeters << "m)";
        }
        std::cout << "\n";
    }
    std::cout << "-----------------------------------------------------------\n";
}

int main(int argc, char* argv[]) {
    std::string mapType = "campus";
    std::string mode = "interactive";
    std::string startId = "";
    std::string targetId = "";
    std::string algo = "astar";
    std::string metricStr = "distance";
    std::vector<std::string> blockedRoads;

    // Parse CLI arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--map" && i + 1 < argc) {
            mapType = argv[++i];
        } else if (arg == "--route") {
            mode = "route";
            if (i + 1 < argc) startId = argv[++i];
            if (i + 1 < argc) targetId = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') algo = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') metricStr = argv[++i];
        } else if (arg == "--block" && i + 1 < argc) {
            std::string roadList = argv[++i];
            std::stringstream ss(roadList);
            std::string rId;
            while (std::getline(ss, rId, ',')) {
                if (!rId.empty()) blockedRoads.push_back(rId);
            }
        } else if (arg == "--export-graph") {
            mode = "export-graph";
        } else if (arg == "--benchmark") {
            mode = "benchmark";
            if (i + 1 < argc) startId = argv[++i];
            if (i + 1 < argc) targetId = argv[++i];
        }
    }

    Graph graph;
    if (!loadMap(graph, mapType)) {
        std::cerr << "{\"error\": \"Failed to load map data for: " << mapType << "\"}\n";
        return 1;
    }

    // Apply any specified road blockages
    for (const auto& roadId : blockedRoads) {
        graph.toggleRoadBlock(roadId, true);
    }

    // MODE: EXPORT GRAPH JSON
    if (mode == "export-graph") {
        std::cout << NavigationEngine::exportGraphToJSON(graph) << "\n";
        return 0;
    }

    // MODE: CLI HEADLESS ROUTE (FOR PYTHON WEB API)
    if (mode == "route") {
        PathFinder::CostMetric metric = (metricStr == "time")
            ? PathFinder::CostMetric::FASTEST_TIME
            : PathFinder::CostMetric::SHORTEST_DISTANCE;

        RouteResult res;
        if (algo == "dijkstra") {
            res = PathFinder::findRouteDijkstra(graph, startId, targetId, metric);
        } else {
            res = PathFinder::findRouteAStar(graph, startId, targetId, metric);
        }

        auto directions = NavigationEngine::generateDirections(graph, res);
        std::cout << NavigationEngine::exportRouteToJSON(graph, res, directions) << "\n";
        return 0;
    }

    // MODE: BENCHMARK
    if (mode == "benchmark") {
        if (startId.empty() || targetId.empty()) {
            std::cout << "Usage: --benchmark <startId> <targetId>\n";
            return 1;
        }
        std::cout << "Running 100 iterations comparing A* vs Dijkstra for " << startId << " -> " << targetId << "...\n";
        long long totalAStarTime = 0;
        long long totalDijkstraTime = 0;
        int aStarNodes = 0;
        int dijkstraNodes = 0;

        for (int i = 0; i < 100; ++i) {
            auto rA = PathFinder::findRouteAStar(graph, startId, targetId);
            auto rD = PathFinder::findRouteDijkstra(graph, startId, targetId);
            totalAStarTime += rA.executionMicroseconds;
            totalDijkstraTime += rD.executionMicroseconds;
            aStarNodes = rA.nodesExplored;
            dijkstraNodes = rD.nodesExplored;
        }

        std::cout << "\n=== BENCHMARK RESULTS (100 Iterations) ===\n";
        std::cout << "A* Average Compute Time       : " << (totalAStarTime / 100.0) << " us\n";
        std::cout << "Dijkstra Average Compute Time : " << (totalDijkstraTime / 100.0) << " us\n";
        std::cout << "A* Nodes Explored             : " << aStarNodes << "\n";
        std::cout << "Dijkstra Nodes Explored       : " << dijkstraNodes << "\n";
        double speedup = (dijkstraNodes > 0) ? (100.0 * (dijkstraNodes - aStarNodes) / dijkstraNodes) : 0.0;
        std::cout << "Heuristic Pruning Efficiency  : " << std::fixed << std::setprecision(1) << speedup << "% fewer nodes explored!\n";
        return 0;
    }

    // MODE: INTERACTIVE CLI
    printBanner();
    int choice = 0;

    while (true) {
        std::cout << "\nACTIVE MAP: [" << (mapType == "city" ? "SMART CITY" : "COLLEGE CAMPUS")
                  << "] (" << graph.getVertexCount() << " Landmarks, "
                  << graph.getEdgeCount() << " Road Segments)\n";
        std::cout << "--------------------------------------------------------\n";
        std::cout << "1. Find Route (A* Search - Recommended)\n";
        std::cout << "2. Find Route (Dijkstra Search)\n";
        std::cout << "3. Compare A* vs Dijkstra Side-by-Side (Benchmark)\n";
        std::cout << "4. Toggle Road Blockage (Simulate Detour/Hazard)\n";
        std::cout << "5. Switch Active Map (College Campus <-> Smart City)\n";
        std::cout << "6. List All Landmarks in Active Map\n";
        std::cout << "7. Export Current Map to JSON\n";
        std::cout << "8. Exit\n";
        std::cout << "Select Option [1-8]: ";

        if (!(std::cin >> choice)) {
            break;
        }

        if (choice == 8) {
            std::cout << "Exiting Navigation Engine. Goodbye!\n";
            break;
        }

        if (choice == 5) {
            mapType = (mapType == "campus") ? "city" : "campus";
            graph = Graph();
            loadMap(graph, mapType);
            std::cout << "Switched map to: " << (mapType == "city" ? "Smart City" : "College Campus") << "\n";
            continue;
        }

        if (choice == 6) {
            std::cout << "\n--- Landmark Directory ---\n";
            for (const auto& pair : graph.getLandmarks()) {
                const auto& lm = pair.second;
                std::cout << std::setw(6) << std::left << lm.id << " | "
                          << std::setw(12) << lm.category << " | " << lm.name << "\n";
            }
            continue;
        }

        if (choice == 4) {
            std::string roadId;
            int blockVal;
            std::cout << "Enter Road Segment ID to toggle (e.g. E05, CE12): ";
            std::cin >> roadId;
            std::cout << "Enter state (1 = Block Road, 0 = Clear Road): ";
            std::cin >> blockVal;
            bool success = graph.toggleRoadBlock(roadId, blockVal == 1);
            if (success) {
                std::cout << "Road " << roadId << " status updated to: " << (blockVal == 1 ? "BLOCKED" : "OPEN") << "\n";
            } else {
                std::cout << "Road ID " << roadId << " not found!\n";
            }
            continue;
        }

        if (choice == 1 || choice == 2 || choice == 3) {
            std::string s, t;
            std::cout << "Enter Origin Landmark ID (e.g., " << (mapType == "city" ? "AIR, STN, DTN" : "G1, CS, H1") << "): ";
            std::cin >> s;
            std::cout << "Enter Destination Landmark ID (e.g., " << (mapType == "city" ? "PORT, EXPO, HOSP2" : "G2, ME, POOL") << "): ";
            std::cin >> t;

            if (!graph.hasLandmark(s) || !graph.hasLandmark(t)) {
                std::cout << "Invalid Landmark IDs entered. Please check option 6 for list.\n";
                continue;
            }

            if (choice == 1) {
                auto res = PathFinder::findRouteAStar(graph, s, t);
                auto directions = NavigationEngine::generateDirections(graph, res);
                printRouteSummary(graph, res, directions);
            } else if (choice == 2) {
                auto res = PathFinder::findRouteDijkstra(graph, s, t);
                auto directions = NavigationEngine::generateDirections(graph, res);
                printRouteSummary(graph, res, directions);
            } else if (choice == 3) {
                auto resA = PathFinder::findRouteAStar(graph, s, t);
                auto resD = PathFinder::findRouteDijkstra(graph, s, t);

                std::cout << "\n================ BENCHMARK COMPARISON ================\n";
                std::cout << "Metric                 | A* Algorithm       | Dijkstra Algorithm \n";
                std::cout << "-----------------------+--------------------+--------------------\n";
                std::cout << "Execution Time         | " << std::setw(15) << (std::to_string(resA.executionMicroseconds) + " us")
                          << " | " << std::setw(15) << (std::to_string(resD.executionMicroseconds) + " us") << "\n";
                std::cout << "Nodes Explored         | " << std::setw(18) << resA.nodesExplored
                          << " | " << std::setw(18) << resD.nodesExplored << "\n";
                std::cout << "Total Distance (m)     | " << std::setw(18) << std::fixed << std::setprecision(1) << resA.totalDistanceMeters
                          << " | " << std::setw(18) << resD.totalDistanceMeters << "\n";
                std::cout << "Path Nodes Count       | " << std::setw(18) << resA.pathNodes.size()
                          << " | " << std::setw(18) << resD.pathNodes.size() << "\n";
                std::cout << "Optimality Check       | " << (std::fabs(resA.totalDistanceMeters - resD.totalDistanceMeters) < 0.1 ? "IDENTICAL SHORTEST" : "DIFFERENT")
                          << "   | " << (std::fabs(resA.totalDistanceMeters - resD.totalDistanceMeters) < 0.1 ? "IDENTICAL SHORTEST" : "DIFFERENT") << "\n";
                std::cout << "======================================================\n";
            }
            continue;
        }

        if (choice == 7) {
            std::string jsonStr = NavigationEngine::exportGraphToJSON(graph);
            std::cout << "\n--- Graph JSON Preview (First 400 chars) ---\n";
            std::cout << jsonStr.substr(0, 400) << "...\n";
            std::cout << "(Use web frontend or --export-graph to consume full JSON)\n";
            continue;
        }
    }

    return 0;
}

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

// ---------------------------------------------------------------------------
// Maps are listed in data/maps.csv (id,label). Each map id has two files:
//   data/<id>_nodes.csv   and   data/<id>_edges.csv
// Adding a map means adding one line and two CSV files; no code changes.
// ---------------------------------------------------------------------------

struct MapInfo {
    std::string id;
    std::string label;
};

static const int BENCHMARK_RUNS = 100;

// The binary may be run from projectsite/ or from its parent folder.
static std::string findDataDir() {
    const char* candidates[] = {"data/", "projectsite/data/"};
    for (const char* dir : candidates) {
        std::ifstream probe(std::string(dir) + "maps.csv");
        if (probe.is_open()) return dir;
    }
    return "data/";
}

static std::vector<MapInfo> loadMapList(const std::string& dataDir) {
    std::vector<MapInfo> maps;
    std::ifstream file(dataDir + "maps.csv");
    std::string line;
    std::getline(file, line); // header
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::stringstream ss(line);
        MapInfo m;
        std::getline(ss, m.id, ',');
        std::getline(ss, m.label, ',');
        if (m.label.empty()) m.label = m.id;
        if (!m.id.empty()) maps.push_back(m);
    }
    return maps;
}

static bool loadMap(Graph& g, const std::string& dataDir, const std::string& mapId) {
    return g.loadFromCSV(dataDir + mapId + "_nodes.csv", dataDir + mapId + "_edges.csv");
}

static std::string labelFor(const std::vector<MapInfo>& maps, const std::string& id) {
    for (const auto& m : maps) if (m.id == id) return m.label;
    return id;
}

static std::string formatDistance(double metres) {
    std::ostringstream ss;
    if (metres >= 1000.0) ss << std::fixed << std::setprecision(2) << metres / 1000.0 << " km";
    else ss << std::fixed << std::setprecision(0) << metres << " m";
    return ss.str();
}

static std::string formatDuration(double seconds) {
    std::ostringstream ss;
    int total = static_cast<int>(std::round(seconds));
    if (total < 60) ss << total << " s";
    else ss << total / 60 << " min " << total % 60 << " s";
    return ss.str();
}

// A few real IDs from the loaded map, used as examples in prompts.
static std::string exampleIds(const Graph& g, size_t count = 3) {
    std::string out;
    size_t n = 0;
    for (const auto& p : g.getLandmarks()) {
        if (n++ >= count) break;
        if (!out.empty()) out += ", ";
        out += p.first;
    }
    return out;
}

static void printRoute(const Graph& graph, const RouteResult& res, const std::vector<TurnInstruction>& steps) {
    std::cout << "\n";
    if (!res.success) {
        std::cout << "No route found. " << res.statusMessage << "\n";
        return;
    }
    std::cout << graph.getLandmark(res.startId).name << "  ->  " << graph.getLandmark(res.targetId).name << "\n";
    std::cout << "  Distance       " << formatDistance(res.totalDistanceMeters) << "\n";
    std::cout << "  Travel time    " << formatDuration(res.totalTimeSeconds) << " at the speed limits\n";
    std::cout << "  Search method  " << res.algorithmUsed << "\n";
    std::cout << "  Places checked " << res.nodesExplored << "\n";
    std::cout << "  Search took    " << res.executionMicroseconds << " microseconds\n\n";
    for (const auto& d : steps) {
        std::cout << "  " << std::setw(2) << d.stepNumber << ". " << d.detailedText;
        if (d.segmentDistanceMeters > 0) std::cout << "  (" << formatDistance(d.segmentDistanceMeters) << ")";
        std::cout << "\n";
    }
}

static void printUsage() {
    std::cout <<
        "Usage:\n"
        "  nav_engine                                   interactive menu\n"
        "  nav_engine --map <id> --route <from> <to> [astar|dijkstra] [distance|time] [--block R1,R2]\n"
        "  nav_engine --map <id> --benchmark <from> <to>\n"
        "  nav_engine --map <id> --export-graph\n"
        "  nav_engine --list-maps\n";
}

int main(int argc, char* argv[]) {
    const std::string dataDir = findDataDir();
    const std::vector<MapInfo> maps = loadMapList(dataDir);
    if (maps.empty()) {
        std::cerr << "{\"error\": \"No maps listed in " << dataDir << "maps.csv\"}\n";
        return 1;
    }

    std::string mapId = maps.front().id;
    std::string mode = "interactive";
    std::string startId, targetId;
    std::string algo = "astar";
    std::string metricStr = "distance";
    std::vector<std::string> closedRoads;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--map" && i + 1 < argc) {
            mapId = argv[++i];
        } else if (arg == "--route") {
            mode = "route";
            if (i + 1 < argc) startId = argv[++i];
            if (i + 1 < argc) targetId = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') algo = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') metricStr = argv[++i];
        } else if (arg == "--block" && i + 1 < argc) {
            std::stringstream ss(argv[++i]);
            std::string id;
            while (std::getline(ss, id, ',')) if (!id.empty()) closedRoads.push_back(id);
        } else if (arg == "--export-graph") {
            mode = "export-graph";
        } else if (arg == "--benchmark") {
            mode = "benchmark";
            if (i + 1 < argc) startId = argv[++i];
            if (i + 1 < argc) targetId = argv[++i];
        } else if (arg == "--list-maps") {
            for (const auto& m : maps) std::cout << m.id << "," << m.label << "\n";
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        }
    }

    Graph graph;
    if (!loadMap(graph, dataDir, mapId)) {
        std::cerr << "{\"error\": \"Could not load the map '" << mapId << "'\"}\n";
        return 1;
    }
    for (const auto& id : closedRoads) graph.toggleRoadBlock(id, true);

    const PathFinder::CostMetric metric = (metricStr == "time")
        ? PathFinder::CostMetric::FASTEST_TIME
        : PathFinder::CostMetric::SHORTEST_DISTANCE;

    if (mode == "export-graph") {
        std::cout << NavigationEngine::exportGraphToJSON(graph) << "\n";
        return 0;
    }

    if (mode == "route") {
        RouteResult res = (algo == "dijkstra")
            ? PathFinder::findRouteDijkstra(graph, startId, targetId, metric)
            : PathFinder::findRouteAStar(graph, startId, targetId, metric);
        auto steps = NavigationEngine::generateDirections(graph, res);
        std::cout << NavigationEngine::exportRouteToJSON(graph, res, steps) << "\n";
        return 0;
    }

    if (mode == "benchmark") {
        if (startId.empty() || targetId.empty()) {
            printUsage();
            return 1;
        }
        long long aTime = 0, dTime = 0;
        int aChecked = 0, dChecked = 0;
        double aDist = 0, dDist = 0;
        for (int i = 0; i < BENCHMARK_RUNS; ++i) {
            auto a = PathFinder::findRouteAStar(graph, startId, targetId, metric);
            auto d = PathFinder::findRouteDijkstra(graph, startId, targetId, metric);
            aTime += a.executionMicroseconds;
            dTime += d.executionMicroseconds;
            aChecked = a.nodesExplored;
            dChecked = d.nodesExplored;
            aDist = a.totalDistanceMeters;
            dDist = d.totalDistanceMeters;
        }
        std::cout << "Average over " << BENCHMARK_RUNS << " runs, " << startId << " to " << targetId << "\n";
        std::cout << "  A*        " << std::setw(8) << (aTime / double(BENCHMARK_RUNS)) << " us   " << aChecked << " places checked\n";
        std::cout << "  Dijkstra  " << std::setw(8) << (dTime / double(BENCHMARK_RUNS)) << " us   " << dChecked << " places checked\n";
        if (dChecked > 0) {
            std::cout << "  A* checked " << std::fixed << std::setprecision(0)
                      << 100.0 * (dChecked - aChecked) / dChecked << "% fewer places";
            std::cout << (std::fabs(aDist - dDist) < 0.5 ? " and found the same route.\n" : ".\n");
        }
        return 0;
    }

    // ---------------- Interactive menu ----------------
    std::cout << "NaviCore route planner\n";
    while (true) {
        std::cout << "\n" << labelFor(maps, mapId) << " map  -  "
                  << graph.getVertexCount() << " places, " << graph.getEdgeCount() << " roads\n"
                  << "  1  Find a route (A*)\n"
                  << "  2  Find a route (Dijkstra)\n"
                  << "  3  Compare A* and Dijkstra\n"
                  << "  4  Close or reopen a road\n"
                  << "  5  Switch map\n"
                  << "  6  List places\n"
                  << "  7  Print the map as JSON\n"
                  << "  8  Quit\n"
                  << "Choose 1-8: ";

        int choice = 0;
        if (!(std::cin >> choice)) break;

        if (choice == 8) break;

        if (choice == 5) {
            std::cout << "Maps:";
            for (const auto& m : maps) std::cout << "  " << m.id;
            std::cout << "\nMap id: ";
            std::string next;
            std::cin >> next;
            Graph fresh;
            if (loadMap(fresh, dataDir, next)) {
                graph = fresh;
                mapId = next;
            } else {
                std::cout << "There is no map called '" << next << "'.\n";
            }
            continue;
        }

        if (choice == 6) {
            for (const auto& p : graph.getLandmarks()) {
                std::cout << "  " << std::setw(6) << std::left << p.second.id
                          << std::setw(12) << p.second.category << p.second.name << "\n";
            }
            std::cout << std::right;
            continue;
        }

        if (choice == 4) {
            std::string roadId;
            int close = 1;
            std::cout << "Road id (for example " << graph.getAllEdges().front().id << "): ";
            std::cin >> roadId;
            std::cout << "1 to close it, 0 to reopen it: ";
            std::cin >> close;
            if (graph.toggleRoadBlock(roadId, close == 1)) {
                std::cout << "Road " << roadId << (close == 1 ? " is closed.\n" : " is open again.\n");
            } else {
                std::cout << "There is no road " << roadId << " on this map.\n";
            }
            continue;
        }

        if (choice >= 1 && choice <= 3) {
            std::string s, t;
            std::cout << "From (for example " << exampleIds(graph) << "): ";
            std::cin >> s;
            std::cout << "To: ";
            std::cin >> t;
            if (!graph.hasLandmark(s) || !graph.hasLandmark(t)) {
                std::cout << "One of those places isn't on this map. Choose 6 to see the list.\n";
                continue;
            }
            if (choice == 3) {
                auto a = PathFinder::findRouteAStar(graph, s, t);
                auto d = PathFinder::findRouteDijkstra(graph, s, t);
                std::cout << "\n             A*        Dijkstra\n"
                          << "  Checked    " << std::setw(8) << a.nodesExplored << "  " << std::setw(8) << d.nodesExplored << "  places\n"
                          << "  Took       " << std::setw(8) << a.executionMicroseconds << "  " << std::setw(8) << d.executionMicroseconds << "  microseconds\n"
                          << "  Distance   " << std::setw(8) << formatDistance(a.totalDistanceMeters)
                          << "  " << std::setw(8) << formatDistance(d.totalDistanceMeters) << "\n";
                std::cout << (std::fabs(a.totalDistanceMeters - d.totalDistanceMeters) < 0.5
                              ? "  Both found the same shortest route.\n"
                              : "  The routes differ.\n");
                continue;
            }
            auto res = (choice == 1) ? PathFinder::findRouteAStar(graph, s, t)
                                     : PathFinder::findRouteDijkstra(graph, s, t);
            printRoute(graph, res, NavigationEngine::generateDirections(graph, res));
            continue;
        }

        if (choice == 7) {
            std::cout << NavigationEngine::exportGraphToJSON(graph);
            continue;
        }
    }
    return 0;
}

#include "../include/NavigationEngine.h"
#include <cmath>
#include <sstream>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

// Turn classification, in degrees of heading change between two road segments.
const double STRAIGHT_LIMIT_DEG = 25.0;   // under this: keep going straight
const double U_TURN_LIMIT_DEG   = 155.0;  // over this: double back

// Escapes a value for a JSON string literal.
std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    std::ostringstream hex;
                    hex << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                    out += hex.str();
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::string quoted(const std::string& s) { return "\"" + jsonEscape(s) + "\""; }

// Real road length for the segment ending at nodes[i]; falls back to straight-line distance.
double segmentLength(const Graph& graph, const RouteResult& route, size_t i) {
    if (i == 0 || i > route.pathRoads.size()) return 0.0;
    double metres = graph.roadLength(route.pathNodes[i - 1], route.pathRoads[i - 1]);
    if (metres >= 0.0) return metres;
    return PathFinder::calculateEuclideanDistance(graph.getLandmark(route.pathNodes[i - 1]),
                                                  graph.getLandmark(route.pathNodes[i]));
}

} // namespace

std::vector<TurnInstruction> NavigationEngine::generateDirections(const Graph& graph,
                                                                 const RouteResult& route) {
    std::vector<TurnInstruction> instructions;
    if (!route.success || route.pathNodes.empty()) return instructions;

    const auto& nodes = route.pathNodes;
    int stepNum = 1;

    const Landmark& startLm = graph.getLandmark(nodes[0]);
    TurnInstruction depart;
    depart.stepNumber = stepNum++;
    depart.landmarkId = startLm.id;
    depart.landmarkName = startLm.name;
    depart.action = "Start";
    depart.segmentDistanceMeters = 0.0;

    if (nodes.size() == 1) {
        depart.detailedText = "You are already at " + startLm.name + ".";
        instructions.push_back(depart);
        return instructions;
    }

    depart.detailedText = "Leave " + startLm.name + " toward " + graph.getLandmark(nodes[1]).name;
    instructions.push_back(depart);

    for (size_t i = 1; i + 1 < nodes.size(); ++i) {
        const Landmark& prev = graph.getLandmark(nodes[i - 1]);
        const Landmark& curr = graph.getLandmark(nodes[i]);
        const Landmark& next = graph.getLandmark(nodes[i + 1]);

        // Heading change via cross and dot products. Screen coordinates have Y pointing down,
        // so a positive cross product is a clockwise (right) turn.
        double v1x = curr.x - prev.x, v1y = curr.y - prev.y;
        double v2x = next.x - curr.x, v2y = next.y - curr.y;
        double cross = v1x * v2y - v1y * v2x;
        double dot = v1x * v2x + v1y * v2y;
        double angleDeg = std::atan2(cross, dot) * (180.0 / M_PI);

        std::string action;
        std::string text;
        if (std::fabs(angleDeg) < STRAIGHT_LIMIT_DEG) {
            action = "Go straight";
            text = "Go straight past " + curr.name + " toward " + next.name;
        } else if (std::fabs(angleDeg) > U_TURN_LIMIT_DEG) {
            action = "Make a U-turn";
            text = "Make a U-turn at " + curr.name + " toward " + next.name;
        } else if (angleDeg > 0) {
            action = "Turn right";
            text = "Turn right at " + curr.name + " toward " + next.name;
        } else {
            action = "Turn left";
            text = "Turn left at " + curr.name + " toward " + next.name;
        }

        TurnInstruction step;
        step.stepNumber = stepNum++;
        step.landmarkId = curr.id;
        step.landmarkName = curr.name;
        step.action = action;
        step.segmentDistanceMeters = segmentLength(graph, route, i);
        step.detailedText = text;
        instructions.push_back(step);
    }

    const Landmark& destLm = graph.getLandmark(nodes.back());
    TurnInstruction arrive;
    arrive.stepNumber = stepNum++;
    arrive.landmarkId = destLm.id;
    arrive.landmarkName = destLm.name;
    arrive.action = "Arrive";
    arrive.segmentDistanceMeters = segmentLength(graph, route, nodes.size() - 1);
    arrive.detailedText = "Arrive at " + destLm.name;
    instructions.push_back(arrive);

    return instructions;
}

std::string NavigationEngine::exportRouteToJSON(const Graph& graph,
                                               const RouteResult& route,
                                               const std::vector<TurnInstruction>& directions) {
    (void)graph;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "{\n";
    ss << "  \"success\": " << (route.success ? "true" : "false") << ",\n";
    ss << "  \"algorithm\": " << quoted(route.algorithmUsed) << ",\n";
    ss << "  \"start\": " << quoted(route.startId) << ",\n";
    ss << "  \"target\": " << quoted(route.targetId) << ",\n";
    ss << "  \"totalDistance\": " << route.totalDistanceMeters << ",\n";
    ss << "  \"totalTimeSeconds\": " << route.totalTimeSeconds << ",\n";
    ss << "  \"nodesExplored\": " << route.nodesExplored << ",\n";
    ss << "  \"executionMicroseconds\": " << route.executionMicroseconds << ",\n";
    ss << "  \"statusMessage\": " << quoted(route.statusMessage) << ",\n";

    ss << "  \"pathNodes\": [";
    for (size_t i = 0; i < route.pathNodes.size(); ++i) {
        ss << quoted(route.pathNodes[i]) << (i + 1 < route.pathNodes.size() ? ", " : "");
    }
    ss << "],\n";

    ss << "  \"pathRoads\": [";
    for (size_t i = 0; i < route.pathRoads.size(); ++i) {
        ss << quoted(route.pathRoads[i]) << (i + 1 < route.pathRoads.size() ? ", " : "");
    }
    ss << "],\n";

    ss << "  \"directions\": [\n";
    for (size_t i = 0; i < directions.size(); ++i) {
        const auto& d = directions[i];
        ss << "    {\"step\": " << d.stepNumber
           << ", \"landmarkId\": " << quoted(d.landmarkId)
           << ", \"landmarkName\": " << quoted(d.landmarkName)
           << ", \"action\": " << quoted(d.action)
           << ", \"instruction\": " << quoted(d.detailedText)
           << ", \"segmentDistance\": " << d.segmentDistanceMeters << "}"
           << (i + 1 < directions.size() ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

std::string NavigationEngine::exportGraphToJSON(const Graph& graph) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "{\n";

    ss << "  \"nodes\": [\n";
    const auto& lmMap = graph.getLandmarks();
    size_t count = 0;
    for (const auto& pair : lmMap) {
        const auto& lm = pair.second;
        ss << "    {\"id\": " << quoted(lm.id) << ", \"name\": " << quoted(lm.name)
           << ", \"x\": " << lm.x << ", \"y\": " << lm.y
           << ", \"category\": " << quoted(lm.category) << "}";
        if (++count < lmMap.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";

    ss << "  \"edges\": [\n";
    const auto& edges = graph.getAllEdges();
    for (size_t i = 0; i < edges.size(); ++i) {
        const auto& e = edges[i];
        ss << "    {\"id\": " << quoted(e.id) << ", \"from\": " << quoted(e.from)
           << ", \"to\": " << quoted(e.to) << ", \"distance\": " << e.distance
           << ", \"speedLimit\": " << e.speedLimit
           << ", \"isOneWay\": " << (e.isOneWay ? "true" : "false")
           << ", \"isBlocked\": " << (e.isBlocked ? "true" : "false") << "}";
        if (i + 1 < edges.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

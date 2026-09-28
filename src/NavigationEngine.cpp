#include "../include/NavigationEngine.h"
#include <cmath>
#include <sstream>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

std::vector<TurnInstruction> NavigationEngine::generateDirections(const Graph& graph,
                                                                 const RouteResult& route) {
    std::vector<TurnInstruction> instructions;
    if (!route.success || route.pathNodes.empty()) return instructions;

    const auto& nodes = route.pathNodes;
    int stepNum = 1;

    // Step 1: Departure
    const Landmark& startLm = graph.getLandmark(nodes[0]);
    TurnInstruction depart;
    depart.stepNumber = stepNum++;
    depart.landmarkId = startLm.id;
    depart.landmarkName = startLm.name;
    depart.action = "Depart";
    depart.segmentDistanceMeters = 0.0;

    if (nodes.size() == 1) {
        depart.detailedText = "You are already at your destination: " + startLm.name;
        instructions.push_back(depart);
        return instructions;
    }

    const Landmark& firstNext = graph.getLandmark(nodes[1]);
    depart.detailedText = "Depart from " + startLm.name + " heading towards " + firstNext.name;
    instructions.push_back(depart);

    // Intermediate turn steps
    for (size_t i = 1; i + 1 < nodes.size(); ++i) {
        const Landmark& prev = graph.getLandmark(nodes[i - 1]);
        const Landmark& curr = graph.getLandmark(nodes[i]);
        const Landmark& next = graph.getLandmark(nodes[i + 1]);

        double v1x = curr.x - prev.x;
        double v1y = curr.y - prev.y;
        double v2x = next.x - curr.x;
        double v2y = next.y - curr.y;

        // In screen/SVG coordinates (Y down):
        // cross > 0: Clockwise (Turn Right)
        // cross < 0: Counter-Clockwise (Turn Left)
        double cross = v1x * v2y - v1y * v2x;
        double dot = v1x * v2x + v1y * v2y;
        double angleRad = std::atan2(cross, dot);
        double angleDeg = angleRad * (180.0 / M_PI);

        std::string action;
        if (std::abs(angleDeg) < 25.0) {
            action = "Continue Straight";
        } else if (angleDeg > 25.0 && angleDeg < 155.0) {
            action = "Turn Right";
        } else if (angleDeg < -25.0 && angleDeg > -155.0) {
            action = "Turn Left";
        } else {
            action = "Make a U-Turn";
        }

        TurnInstruction step;
        step.stepNumber = stepNum++;
        step.landmarkId = curr.id;
        step.landmarkName = curr.name;
        step.action = action;
        step.segmentDistanceMeters = PathFinder::calculateEuclideanDistance(prev, curr);
        step.detailedText = "At " + curr.name + ", " + action + " towards " + next.name;
        instructions.push_back(step);
    }

    // Final step: Arrival
    const Landmark& destLm = graph.getLandmark(nodes.back());
    const Landmark& secondLastLm = graph.getLandmark(nodes[nodes.size() - 2]);
    TurnInstruction arrive;
    arrive.stepNumber = stepNum++;
    arrive.landmarkId = destLm.id;
    arrive.landmarkName = destLm.name;
    arrive.action = "Arrive";
    arrive.segmentDistanceMeters = PathFinder::calculateEuclideanDistance(secondLastLm, destLm);
    arrive.detailedText = "Arrive at destination: " + destLm.name;
    instructions.push_back(arrive);

    return instructions;
}

std::string NavigationEngine::exportRouteToJSON(const Graph& graph,
                                               const RouteResult& route,
                                               const std::vector<TurnInstruction>& directions) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "{\n";
    ss << "  \"success\": " << (route.success ? "true" : "false") << ",\n";
    ss << "  \"algorithm\": \"" << route.algorithmUsed << "\",\n";
    ss << "  \"start\": \"" << route.startId << "\",\n";
    ss << "  \"target\": \"" << route.targetId << "\",\n";
    ss << "  \"totalDistance\": " << route.totalDistanceMeters << ",\n";
    ss << "  \"totalTimeSeconds\": " << route.totalTimeSeconds << ",\n";
    ss << "  \"nodesExplored\": " << route.nodesExplored << ",\n";
    ss << "  \"executionMicroseconds\": " << route.executionMicroseconds << ",\n";
    ss << "  \"statusMessage\": \"" << route.statusMessage << "\",\n";

    // Path nodes
    ss << "  \"pathNodes\": [";
    for (size_t i = 0; i < route.pathNodes.size(); ++i) {
        ss << "\"" << route.pathNodes[i] << "\"";
        if (i + 1 < route.pathNodes.size()) ss << ", ";
    }
    ss << "],\n";

    // Path roads
    ss << "  \"pathRoads\": [";
    for (size_t i = 0; i < route.pathRoads.size(); ++i) {
        ss << "\"" << route.pathRoads[i] << "\"";
        if (i + 1 < route.pathRoads.size()) ss << ", ";
    }
    ss << "],\n";

    // Turn-by-turn guidance
    ss << "  \"directions\": [\n";
    for (size_t i = 0; i < directions.size(); ++i) {
        const auto& d = directions[i];
        ss << "    {\n";
        ss << "      \"step\": " << d.stepNumber << ",\n";
        ss << "      \"landmarkId\": \"" << d.landmarkId << "\",\n";
        ss << "      \"landmarkName\": \"" << d.landmarkName << "\",\n";
        ss << "      \"action\": \"" << d.action << "\",\n";
        ss << "      \"instruction\": \"" << d.detailedText << "\",\n";
        ss << "      \"segmentDistance\": " << d.segmentDistanceMeters << "\n";
        ss << "    }" << (i + 1 < directions.size() ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

std::string NavigationEngine::exportGraphToJSON(const Graph& graph) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "{\n";
    
    // Landmarks
    ss << "  \"nodes\": [\n";
    const auto& lmMap = graph.getLandmarks();
    size_t count = 0;
    for (const auto& pair : lmMap) {
        const auto& lm = pair.second;
        ss << "    {\"id\": \"" << lm.id << "\", \"name\": \"" << lm.name
           << "\", \"x\": " << lm.x << ", \"y\": " << lm.y
           << ", \"category\": \"" << lm.category << "\"}";
        if (++count < lmMap.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";

    // Edges
    ss << "  \"edges\": [\n";
    const auto& edges = graph.getAllEdges();
    for (size_t i = 0; i < edges.size(); ++i) {
        const auto& e = edges[i];
        ss << "    {\"id\": \"" << e.id << "\", \"from\": \"" << e.from
           << "\", \"to\": \"" << e.to << "\", \"distance\": " << e.distance
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

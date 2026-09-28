#include "../include/Graph.h"
#include <fstream>
#include <sstream>
#include <iostream>

Graph::Graph() {}

void Graph::addLandmark(const Landmark& lm) {
    landmarks[lm.id] = lm;
    if (adjList.find(lm.id) == adjList.end()) {
        adjList[lm.id] = std::vector<Edge>();
    }
}

void Graph::addEdge(const Edge& edge) {
    allEdges.push_back(edge);
    adjList[edge.from].push_back(edge);

    if (!edge.isOneWay) {
        Edge reverseEdge = edge;
        reverseEdge.from = edge.to;
        reverseEdge.to = edge.from;
        adjList[edge.to].push_back(reverseEdge);
    }
}

bool Graph::loadFromCSV(const std::string& nodesCsvPath, const std::string& edgesCsvPath) {
    // 1. Load Nodes
    std::ifstream nodesFile(nodesCsvPath);
    if (!nodesFile.is_open()) {
        std::cerr << "Error: Could not open nodes CSV at: " << nodesCsvPath << "\n";
        return false;
    }

    std::string line;
    std::getline(nodesFile, line); // Skip header: id,name,x,y,category

    while (std::getline(nodesFile, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string id, name, xStr, yStr, category;

        if (std::getline(ss, id, ',') &&
            std::getline(ss, name, ',') &&
            std::getline(ss, xStr, ',') &&
            std::getline(ss, yStr, ',') &&
            std::getline(ss, category, ',')) {
            double x = std::stod(xStr);
            double y = std::stod(yStr);
            addLandmark(Landmark(id, name, x, y, category));
        }
    }
    nodesFile.close();

    // 2. Load Edges
    std::ifstream edgesFile(edgesCsvPath);
    if (!edgesFile.is_open()) {
        std::cerr << "Error: Could not open edges CSV at: " << edgesCsvPath << "\n";
        return false;
    }

    std::getline(edgesFile, line); // Skip header: id,from,to,distance,speed_limit,is_oneway

    while (std::getline(edgesFile, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string id, from, to, distStr, speedStr, oneWayStr;

        if (std::getline(ss, id, ',') &&
            std::getline(ss, from, ',') &&
            std::getline(ss, to, ',') &&
            std::getline(ss, distStr, ',') &&
            std::getline(ss, speedStr, ',') &&
            std::getline(ss, oneWayStr, ',')) {
            double dist = std::stod(distStr);
            double speed = std::stod(speedStr);
            bool isOneWay = (oneWayStr == "true" || oneWayStr == "1");
            addEdge(Edge(id, from, to, dist, speed, isOneWay, false));
        }
    }
    edgesFile.close();
    return true;
}

bool Graph::toggleRoadBlock(const std::string& roadId, bool blockStatus) {
    bool found = false;
    for (auto& edge : allEdges) {
        if (edge.id == roadId) {
            edge.isBlocked = blockStatus;
            found = true;
            break;
        }
    }

    for (auto& pair : adjList) {
        for (auto& edge : pair.second) {
            if (edge.id == roadId) {
                edge.isBlocked = blockStatus;
                found = true;
            }
        }
    }
    return found;
}

bool Graph::setRoadBlockBetween(const std::string& u, const std::string& v, bool blockStatus) {
    bool found = false;
    for (auto& pair : adjList) {
        for (auto& edge : pair.second) {
            if ((edge.from == u && edge.to == v) || (edge.from == v && edge.to == u)) {
                edge.isBlocked = blockStatus;
                found = true;
            }
        }
    }
    for (auto& edge : allEdges) {
        if ((edge.from == u && edge.to == v) || (edge.from == v && edge.to == u)) {
            edge.isBlocked = blockStatus;
            found = true;
        }
    }
    return found;
}

const std::unordered_map<std::string, Landmark>& Graph::getLandmarks() const {
    return landmarks;
}

const std::vector<Edge>& Graph::getNeighbors(const std::string& nodeId) const {
    static const std::vector<Edge> emptyVec;
    auto it = adjList.find(nodeId);
    if (it != adjList.end()) return it->second;
    return emptyVec;
}

const std::vector<Edge>& Graph::getAllEdges() const {
    return allEdges;
}

bool Graph::hasLandmark(const std::string& id) const {
    return landmarks.find(id) != landmarks.end();
}

const Landmark& Graph::getLandmark(const std::string& id) const {
    static const Landmark defaultLandmark;
    auto it = landmarks.find(id);
    if (it != landmarks.end()) return it->second;
    return defaultLandmark;
}

#ifndef GRAPH_H
#define GRAPH_H

#include "Landmark.h"
#include "Edge.h"
#include <unordered_map>
#include <vector>
#include <string>

class Graph {
private:
    std::unordered_map<std::string, Landmark> landmarks;
    std::unordered_map<std::string, std::vector<Edge>> adjList;
    std::vector<Edge> allEdges;

public:
    Graph();

    bool loadFromCSV(const std::string& nodesCsvPath, const std::string& edgesCsvPath);

    void addLandmark(const Landmark& lm);
    void addEdge(const Edge& edge);

    bool toggleRoadBlock(const std::string& roadId, bool blockStatus);
    bool setRoadBlockBetween(const std::string& u, const std::string& v, bool blockStatus);

    const std::unordered_map<std::string, Landmark>& getLandmarks() const;
    const std::vector<Edge>& getNeighbors(const std::string& nodeId) const;
    const std::vector<Edge>& getAllEdges() const;

    bool hasLandmark(const std::string& id) const;
    const Landmark& getLandmark(const std::string& id) const;

    int getVertexCount() const { return static_cast<int>(landmarks.size()); }
    int getEdgeCount() const { return static_cast<int>(allEdges.size()); }
};

#endif // GRAPH_H

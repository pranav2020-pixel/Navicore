#ifndef EDGE_H
#define EDGE_H

#include <string>

struct Edge {
    std::string id;          // Unique road ID, e.g. "e1"
    std::string from;        // Source node ID
    std::string to;          // Destination node ID
    double distance;         // Physical distance in meters
    double speedLimit;       // Speed limit in km/h
    bool isOneWay;           // Directionality flag
    bool isBlocked;          // Dynamic obstacle avoidance flag (true = road under construction)

    Edge() : id(""), from(""), to(""), distance(0.0), speedLimit(20.0), isOneWay(false), isBlocked(false) {}

    Edge(std::string id_, std::string from_, std::string to_, double dist_, double speed_, bool oneway_ = false, bool blocked_ = false)
        : id(id_), from(from_), to(to_), distance(dist_), speedLimit(speed_), isOneWay(oneway_), isBlocked(blocked_) {}

    // Computes transit time in seconds: t = d / (speed_kmh * 1000 / 3600)
    double travelTimeSeconds() const {
        if (speedLimit <= 0.0) return distance / 1.4; // Default walking speed ~1.4 m/s
        return (distance / (speedLimit * 1000.0 / 3600.0));
    }
};

#endif // EDGE_H

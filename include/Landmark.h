#ifndef LANDMARK_H
#define LANDMARK_H

#include <string>

struct Landmark {
    std::string id;          // Unique identifier, e.g. "LIB"
    std::string name;        // Full readable name, e.g. "Central Library"
    double x;                // 2D coordinate X (meters/pixels)
    double y;                // 2D coordinate Y (meters/pixels)
    std::string category;    // "Gate", "Academic", "Cafeteria", "Facility", "Residential"

    Landmark() : id(""), name(""), x(0.0), y(0.0), category("") {}
    Landmark(std::string id_, std::string name_, double x_, double y_, std::string cat_)
        : id(id_), name(name_), x(x_), y(y_), category(cat_) {}
};

#endif // LANDMARK_H

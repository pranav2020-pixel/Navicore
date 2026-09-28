# 🚀 NaviCore: Dual-Map Campus & Smart City Navigation System

[![Live Demo](https://img.shields.io/badge/Live_Demo-Interactive_Web_App-0284c7?style=for-the-badge&logo=googlechrome&logoColor=white)](https://pranav2020-pixel.github.io/Navicore/)
[![GitHub Pages](https://img.shields.io/badge/Deploy-GitHub_Pages-brightgreen?style=for-the-badge&logo=github)](https://pranav2020-pixel.github.io/Navicore/)
[![C++14](https://img.shields.io/badge/Language-C++14-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![Algorithms](https://img.shields.io/badge/DSA-A*_&_Dijkstra-f59e0b?style=for-the-badge)](https://en.wikipedia.org/wiki/A*_search_algorithm)

> 🌐 **Live Interactive Web Demo**: **[Launch Live Navigation Map & Routing Demo](https://pranav2020-pixel.github.io/Navicore/)**
> 
> *Test live shortest path routing, switch between College Campus (28 nodes) and Smart City (32 nodes), click on roads to trigger dynamic hazard detours, and benchmark A\* vs. Dijkstra directly in your browser!*

---

## 🌟 Key Highlights

- **Dual-Map Architecture**:
  - 🎓 **College Campus Map**: 28 Realistic Landmarks (Academic blocks, hostels, athletic complexes, cafeterias, libraries, gates) connected by 45 paths.
  - 🏙️ **Smart City Metropolitan Map**: 32 Major Urban Zones (International Airport, Silicon Cyber Towers, Central Train & Metro Hub, Skyline River Suspension Bridge, Apex Hospitals, Olympic Arena, Harbor Port) connected by 53 arterial roads and expressways.
- **Algorithms Implemented from Scratch**:
  - **A\* (A-Star)** with Euclidean Admissible Heuristic $h(n)$ for search space pruning.
  - **Dijkstra's Algorithm** with Min-Heap Priority Queue ($\mathcal{O}((V + E) \log V)$).
- **Dynamic Detours & Hazard Simulation**:
  - Click any road on the map to block it (simulate road construction, accidents, or closures).
  - The engine instantly recalculates the optimal alternative detour path in microseconds.
- **Turn-by-Turn Natural Language Guidance**:
  - Vector cross-product orientation determines direction (`Depart`, `Turn Left`, `Turn Right`, `Continue Straight`, `Arrive`) with segment distances.
- **Visual Web Frontend**:
  - Interactive SVG canvas with Zoom, Pan, responsive markers, and glowing animated route paths.
  - Real-time performance HUD showing Distance, Travel Time, Nodes Explored, and C++ Latency in $\mu s$.
  - Side-by-side benchmark comparison drawer.

---

## 📁 Directory Structure

```
projectsite/
├── data/
│   ├── campus_nodes.csv     # 28 Campus Landmarks (id, name, x, y, category)
│   ├── campus_edges.csv     # 45 Campus Road Paths (id, from, to, distance, speed, oneway)
│   ├── city_nodes.csv       # 32 Smart City Zones
│   ├── city_edges.csv       # 53 Arterial Roads and Expressways
│   ├── nodes.csv            # Default active nodes copy
│   └── edges.csv            # Default active edges copy
├── include/
│   ├── Landmark.h           # Node / Landmark struct definition
│   ├── Edge.h               # Weighted directed/undirected edge with transit time
│   ├── Graph.h              # Graph representation with Adjacency List & CSV loader
│   ├── PathFinder.h         # Dijkstra and A* pathfinding solvers
│   └── NavigationEngine.h   # Turn guidance generator & JSON exporter
├── src/
│   ├── Graph.cpp            # Adjacency list and blockage toggling
│   ├── PathFinder.cpp       # Min-heap priority queue implementations
│   ├── NavigationEngine.cpp # Vector geometry for turn calculation & JSON serialization
│   └── main.cpp             # CLI entrypoint (Dual CLI API + Interactive Menu)
├── web/
│   ├── index.html           # Modern dark UI layout
│   ├── style.css            # Glassmorphism, animations, HUD layout
│   ├── app.js               # SVG canvas controller, map switcher, pan/zoom
│   ├── campus_graph.json    # Pre-exported campus graph data
│   └── city_graph.json      # Pre-exported city graph data
├── server.py                # Python HTTP server bridge between frontend & C++ binary
├── nav_engine.exe           # Compiled high-performance C++ binary
├── build.bat                # Windows build script
├── run.bat                  # One-click launch script
└── README.md                # Project documentation
```

---

## ⚡ How to Run

### Method 1: Full Web UI with C++ Engine (Recommended)
1. Double-click `run.bat` (or run in terminal):
   ```bash
   python server.py
   ```
2. Open your web browser to `http://localhost:8000`.
3. Use the toggle buttons at top to switch between **College Campus** and **Smart City**.
4. Click on nodes to set origin and destination, or click on roads to simulate active roadblocks and watch the route dynamically reroute!

### Method 2: Direct Interactive C++ Terminal Menu
Run the compiled binary directly:
```bash
.\nav_engine.exe
```
Provides an interactive text menu with live options:
1. Find Route (A* Search)
2. Find Route (Dijkstra Search)
3. Compare A* vs Dijkstra Side-by-Side (Benchmark)
4. Toggle Road Blockage (Simulate Detour/Hazard)
5. Switch Active Map (College Campus <-> Smart City)
6. List All Landmarks
7. Export Graph to JSON

### Method 3: CLI Query Mode
```bash
# Query Campus Route
.\nav_engine.exe --map campus --route G1 POOL astar distance

# Query City Route with Road Blockage Detour
.\nav_engine.exe --map city --route AIR PORT astar distance --block CE12

# Run 100-Iteration Benchmark
.\nav_engine.exe --map campus --benchmark G1 POOL
```

---

## 📊 Evaluation & Grading Highlights

| Feature | Rubric Target | Project Implementation |
|---|---|---|
| **Graph Modeling** | Dynamic Graph Representation | Adjacency List `unordered_map<string, vector<Edge>>` with $\mathcal{O}(1)$ vertex lookups |
| **Shortest Path** | Advanced Pathfinding | Dijkstra & A* with Euclidean admissible heuristic |
| **Dynamic Obstacles** | Real-world problem solving | Dynamic edge suppression with instantaneous detour recalculation |
| **Real-world Scale** | Non-trivial datasets | Dual-scale: 28-node Campus + 32-node Metropolitan City |
| **Full Stack Delivery** | Presentation & Visuals | C++ backend + Python REST bridge + Modern SVG web frontend |

---





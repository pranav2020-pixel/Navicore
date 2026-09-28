/**
 * NaviCore Navigation Engine - Frontend Controller
 * Connects to C++ backend via server.py with full client-side fallback
 */

let activeMap = 'campus'; // 'campus' | 'city'
let graphData = { nodes: [], edges: [] };
let blockedRoads = new Set();
let selectedStart = '';
let selectedTarget = '';
let isServerConnected = false;

// Zoom and Pan State
let zoomLevel = 1.0;
let panX = 0;
let panY = 0;
let isPanning = false;
let startPanX = 0;
let startPanY = 0;

// Category color palettes
const CATEGORY_COLORS = {
  // Campus Categories
  Gate: '#f59e0b',       // Amber
  Academic: '#38bdf8',   // Sky Blue
  Residential: '#10b981',// Emerald
  Facility: '#c084fc',   // Purple
  Cafeteria: '#fb923c',  // Orange
  Sports: '#06b6d4',     // Cyan
  // City Categories
  Transit: '#06b6d4',    // Cyan
  Business: '#38bdf8',   // Blue
  Public: '#eab308',     // Yellow
  Medical: '#f43f5e',    // Rose
  Recreation: '#10b981'  // Emerald
};

document.addEventListener('DOMContentLoaded', () => {
  initEngineStatus();
  loadMapData(activeMap);
  setupPanZoom();
});

async function initEngineStatus() {
  try {
    const res = await fetch('/api/status', { method: 'GET' });
    if (res.ok) {
      const data = await res.json();
      isServerConnected = true;
      document.getElementById('engineStatusLabel').textContent = 'C++ Engine Connected (Port 8000)';
      document.querySelector('.engine-status').style.borderColor = 'rgba(16, 185, 129, 0.4)';
    }
  } catch (e) {
    isServerConnected = false;
    document.getElementById('engineStatusLabel').textContent = 'Direct Client Engine (Standalone)';
    document.querySelector('.status-dot').style.backgroundColor = '#f59e0b';
  }
}

async function switchMap(mapName) {
  if (activeMap === mapName) return;
  activeMap = mapName;

  document.getElementById('btnMapCampus').classList.toggle('active', mapName === 'campus');
  document.getElementById('btnMapCity').classList.toggle('active', mapName === 'city');

  blockedRoads.clear();
  selectedStart = '';
  selectedTarget = '';
  clearRoute();
  await loadMapData(mapName);
}

async function loadMapData(mapName) {
  try {
    let res = null;
    if (isServerConnected) {
      res = await fetch(`/api/graph?map=${mapName}`);
    }
    if (!res || !res.ok) {
      res = await fetch(`${mapName}_graph.json`);
    }
    graphData = await res.json();
    populateDropdowns();
    renderMap();
    updateHazardUI();
  } catch (err) {
    console.error('Failed to load map data:', err);
  }
}

function populateDropdowns() {
  const startSelect = document.getElementById('selectStart');
  const targetSelect = document.getElementById('selectTarget');
  startSelect.innerHTML = '';
  targetSelect.innerHTML = '';

  // Sort nodes alphabetically by name
  const sortedNodes = [...graphData.nodes].sort((a, b) => a.name.localeCompare(b.name));

  sortedNodes.forEach(node => {
    const optA = document.createElement('option');
    optA.value = node.id;
    optA.textContent = `[${node.id}] ${node.name} (${node.category})`;
    startSelect.appendChild(optA);

    const optB = document.createElement('option');
    optB.value = node.id;
    optB.textContent = `[${node.id}] ${node.name} (${node.category})`;
    targetSelect.appendChild(optB);
  });

  // Sensible default selections
  if (activeMap === 'campus') {
    startSelect.value = 'G1';
    targetSelect.value = 'POOL';
  } else {
    startSelect.value = 'AIR';
    targetSelect.value = 'PORT';
  }

  selectedStart = startSelect.value;
  selectedTarget = targetSelect.value;

  startSelect.onchange = () => {
    selectedStart = startSelect.value;
    updateNodeHighlights();
  };
  targetSelect.onchange = () => {
    selectedTarget = targetSelect.value;
    updateNodeHighlights();
  };
}

function renderMap() {
  const edgesLayer = document.getElementById('edgesLayer');
  const nodesLayer = document.getElementById('nodesLayer');
  const routeLayer = document.getElementById('routeLayer');

  edgesLayer.innerHTML = '';
  nodesLayer.innerHTML = '';
  routeLayer.innerHTML = '';

  const nodeMap = new Map();
  graphData.nodes.forEach(n => nodeMap.set(n.id, n));

  // 1. Draw Edges
  graphData.edges.forEach(edge => {
    const u = nodeMap.get(edge.from);
    const v = nodeMap.get(edge.to);
    if (!u || !v) return;

    const line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
    line.setAttribute('x1', u.x);
    line.setAttribute('y1', u.y);
    line.setAttribute('x2', v.x);
    line.setAttribute('y2', v.y);
    line.setAttribute('class', `map-edge ${blockedRoads.has(edge.id) ? 'blocked' : ''}`);
    line.setAttribute('id', `edge-${edge.id}`);
    line.setAttribute('data-id', edge.id);

    // Click on edge to toggle blocked/detour
    line.addEventListener('click', () => toggleRoadBlock(edge.id));

    // Tooltip
    const title = document.createElementNS('http://www.w3.org/2000/svg', 'title');
    title.textContent = `Road [${edge.id}]: ${u.name} ↔ ${v.name} (${edge.distance}m, ${edge.speedLimit}km/h)`;
    line.appendChild(title);

    edgesLayer.appendChild(line);
  });

  // 2. Draw Nodes
  graphData.nodes.forEach(node => {
    const g = document.createElementNS('http://www.w3.org/2000/svg', 'g');
    g.setAttribute('class', 'node-group');
    g.setAttribute('id', `node-group-${node.id}`);
    g.setAttribute('transform', `translate(${node.x}, ${node.y})`);

    // Halo pulse ring for start/target
    const halo = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
    halo.setAttribute('class', 'node-halo');
    halo.setAttribute('r', '16');
    halo.setAttribute('fill', 'none');
    halo.setAttribute('stroke', 'none');

    // Outer circle
    const outer = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
    outer.setAttribute('r', '10');
    outer.setAttribute('fill', '#111827');
    outer.setAttribute('stroke', CATEGORY_COLORS[node.category] || '#38bdf8');
    outer.setAttribute('stroke-width', '2.5');

    // Inner core
    const core = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
    core.setAttribute('class', 'node-core');
    core.setAttribute('r', '5');
    core.setAttribute('fill', CATEGORY_COLORS[node.category] || '#38bdf8');

    // Label
    const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
    text.setAttribute('class', 'node-label');
    text.setAttribute('y', '-14');
    text.textContent = node.name;

    // Tooltip
    const title = document.createElementNS('http://www.w3.org/2000/svg', 'title');
    title.textContent = `[${node.id}] ${node.name} (${node.category})`;
    g.appendChild(title);

    g.appendChild(halo);
    g.appendChild(outer);
    g.appendChild(core);
    g.appendChild(text);

    // Click to select start/target
    g.addEventListener('click', (e) => {
      e.stopPropagation();
      handleNodeClick(node.id);
    });

    nodesLayer.appendChild(g);
  });

  updateNodeHighlights();
}

function handleNodeClick(nodeId) {
  const startSelect = document.getElementById('selectStart');
  const targetSelect = document.getElementById('selectTarget');

  if (!selectedStart || (selectedStart && selectedTarget)) {
    selectedStart = nodeId;
    selectedTarget = '';
    startSelect.value = nodeId;
    clearRouteDisplayOnly();
  } else if (selectedStart && !selectedTarget) {
    if (selectedStart === nodeId) return; // Same node
    selectedTarget = nodeId;
    targetSelect.value = nodeId;
    calculateRoute();
  }
  updateNodeHighlights();
}

function updateNodeHighlights() {
  document.querySelectorAll('.node-group').forEach(g => {
    g.classList.remove('is-start', 'is-target');
  });

  if (selectedStart) {
    const sEl = document.getElementById(`node-group-${selectedStart}`);
    if (sEl) sEl.classList.add('is-start');
  }
  if (selectedTarget) {
    const tEl = document.getElementById(`node-group-${selectedTarget}`);
    if (tEl) tEl.classList.add('is-target');
  }
}

function toggleRoadBlock(roadId) {
  if (blockedRoads.has(roadId)) {
    blockedRoads.delete(roadId);
  } else {
    blockedRoads.add(roadId);
  }

  // Update SVG line class
  const edgeEl = document.getElementById(`edge-${roadId}`);
  if (edgeEl) {
    edgeEl.classList.toggle('blocked', blockedRoads.has(roadId));
  }

  updateHazardUI();

  // If a route is currently visible, recalculate automatically to show the detour!
  if (selectedStart && selectedTarget) {
    calculateRoute();
  }
}

function updateHazardUI() {
  const countBadge = document.getElementById('hazardCountBadge');
  const container = document.getElementById('hazardListContainer');
  countBadge.textContent = `${blockedRoads.size} Active`;

  if (blockedRoads.size === 0) {
    container.innerHTML = '<div style="color: var(--text-muted); font-size: 0.72rem; font-style: italic;">No active roadblocks. Click any road on the map to block it.</div>';
    return;
  }

  container.innerHTML = '';
  blockedRoads.forEach(roadId => {
    const edge = graphData.edges.find(e => e.id === roadId);
    const item = document.createElement('div');
    item.className = 'hazard-item active';
    item.innerHTML = `
      <span>⚠️ <b>${roadId}</b>: ${edge ? edge.from + ' ↔ ' + edge.to : 'Road'}</span>
      <button class="hazard-btn" onclick="toggleRoadBlock('${roadId}')" title="Clear Blockage">✕</button>
    `;
    container.appendChild(item);
  });
}

async function calculateRoute() {
  selectedStart = document.getElementById('selectStart').value;
  selectedTarget = document.getElementById('selectTarget').value;
  const algo = document.getElementById('selectAlgo').value;
  const metric = document.getElementById('selectMetric').value;

  if (!selectedStart || !selectedTarget) return;

  updateNodeHighlights();

  let routeResult = null;

  if (isServerConnected) {
    try {
      const resp = await fetch('/api/route', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          map: activeMap,
          start: selectedStart,
          target: selectedTarget,
          algo: algo,
          metric: metric,
          blockedRoads: Array.from(blockedRoads)
        })
      });
      if (resp.ok) {
        routeResult = await resp.json();
      }
    } catch (e) {
      console.warn('Backend call failed, using client fallback:', e);
    }
  }

  // Client-side fallback if offline
  if (!routeResult || !routeResult.success) {
    routeResult = solveRouteClient(selectedStart, selectedTarget, algo, metric);
  }

  displayRoute(routeResult);
}

function displayRoute(res) {
  if (!res || !res.success) {
    alert(res ? res.statusMessage : 'No reachable route found due to road closures.');
    return;
  }

  // Update HUD
  document.getElementById('hudDistance').textContent = `${res.totalDistance.toFixed(0)} m`;
  document.getElementById('hudTime').textContent = `${(res.totalTimeSeconds / 60).toFixed(1)} min`;
  document.getElementById('hudNodes').textContent = res.nodesExplored;
  document.getElementById('hudLatency').textContent = `${res.executionMicroseconds} µs`;

  // Draw Route on SVG
  const routeLayer = document.getElementById('routeLayer');
  routeLayer.innerHTML = '';

  const nodeMap = new Map();
  graphData.nodes.forEach(n => nodeMap.set(n.id, n));

  if (res.pathNodes.length > 1) {
    let d = '';
    for (let i = 0; i < res.pathNodes.length; ++i) {
      const lm = nodeMap.get(res.pathNodes[i]);
      if (!lm) continue;
      d += (i === 0 ? `M ${lm.x} ${lm.y} ` : `L ${lm.x} ${lm.y} `);
    }

    // Outer glow track
    const pathBg = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    pathBg.setAttribute('d', d);
    pathBg.setAttribute('class', 'route-path-bg');

    // Inner pulsing animated stroke
    const pathMain = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    pathMain.setAttribute('d', d);
    pathMain.setAttribute('class', 'route-path-main');

    routeLayer.appendChild(pathBg);
    routeLayer.appendChild(pathMain);
  }

  // Update Directions List in Sidebar
  const dirContainer = document.getElementById('directionsList');
  dirContainer.innerHTML = '';

  if (res.directions && res.directions.length > 0) {
    res.directions.forEach(d => {
      const stepEl = document.createElement('div');
      stepEl.className = 'direction-step';

      let actionIcon = '⬆️';
      if (d.action === 'Depart') actionIcon = '🛫';
      else if (d.action === 'Turn Right') actionIcon = '➡️';
      else if (d.action === 'Turn Left') actionIcon = '⬅️';
      else if (d.action === 'Arrive') actionIcon = '🏁';
      else if (d.action.includes('U-Turn')) actionIcon = '🔄';

      stepEl.innerHTML = `
        <span class="step-badge">#${d.step}</span>
        <div class="step-info">
          <span class="step-action">${actionIcon} ${d.action}</span>
          <span class="step-desc">${d.instruction}</span>
        </div>
      `;
      dirContainer.appendChild(stepEl);
    });
  }
}

function clearRouteDisplayOnly() {
  document.getElementById('routeLayer').innerHTML = '';
  document.getElementById('directionsList').innerHTML = '<div class="empty-state">Select destination to compute route.</div>';
  document.getElementById('hudDistance').textContent = '--';
  document.getElementById('hudTime').textContent = '--';
  document.getElementById('hudNodes').textContent = '--';
  document.getElementById('hudLatency').textContent = '--';
}

function clearRoute() {
  clearRouteDisplayOnly();
  blockedRoads.clear();
  document.querySelectorAll('.map-edge').forEach(e => e.classList.remove('blocked'));
  updateHazardUI();
  updateNodeHighlights();
}

async function runBenchmark() {
  selectedStart = document.getElementById('selectStart').value;
  selectedTarget = document.getElementById('selectTarget').value;
  if (!selectedStart || !selectedTarget) return;

  const resA = solveRouteClient(selectedStart, selectedTarget, 'astar', 'distance');
  const resD = solveRouteClient(selectedStart, selectedTarget, 'dijkstra', 'distance');

  const drawer = document.getElementById('comparisonDrawer');
  const table = document.getElementById('benchmarkTable');

  table.innerHTML = `
    <div class="bench-card">
      <div class="bench-card-title">⚡ A* Algorithm (Heuristic Guided)</div>
      <div class="bench-row"><span>Compute Latency</span><span>${resA.executionMicroseconds} µs</span></div>
      <div class="bench-row"><span>Nodes Explored</span><span>${resA.nodesExplored} nodes</span></div>
      <div class="bench-row"><span>Total Distance</span><span>${resA.totalDistance.toFixed(0)} m</span></div>
      <div class="bench-row"><span>Path Hops</span><span>${resA.pathNodes.length} nodes</span></div>
    </div>
    <div class="bench-card">
      <div class="bench-card-title">🔍 Dijkstra (Exhaustive Frontier)</div>
      <div class="bench-row"><span>Compute Latency</span><span>${resD.executionMicroseconds} µs</span></div>
      <div class="bench-row"><span>Nodes Explored</span><span>${resD.nodesExplored} nodes</span></div>
      <div class="bench-row"><span>Total Distance</span><span>${resD.totalDistance.toFixed(0)} m</span></div>
      <div class="bench-row"><span>Path Hops</span><span>${resD.pathNodes.length} nodes</span></div>
    </div>
    <div class="bench-card">
      <div class="bench-card-title">🏆 Efficiency Gain</div>
      <div class="bench-row"><span>Search Space Pruning</span><span style="color: #10b981;">${resD.nodesExplored > 0 ? (((resD.nodesExplored - resA.nodesExplored) / resD.nodesExplored) * 100).toFixed(1) : 0}%</span></div>
      <div class="bench-row"><span>Optimality Verification</span><span style="color: #38bdf8;">${Math.abs(resA.totalDistance - resD.totalDistance) < 1 ? '100% Identical Shortest' : 'Suboptimal'}</span></div>
      <div class="bench-row"><span>Detours Avoided</span><span>${blockedRoads.size} obstacles</span></div>
    </div>
  `;

  drawer.classList.add('open');
}

function closeBenchmarkDrawer() {
  document.getElementById('comparisonDrawer').classList.remove('open');
}

// Client-side fallback solver (in pure JS) ensuring standalone usability
function solveRouteClient(startId, targetId, algo, metric) {
  const t0 = performance.now();
  const nodeMap = new Map();
  graphData.nodes.forEach(n => nodeMap.set(n.id, n));

  const adj = new Map();
  graphData.nodes.forEach(n => adj.set(n.id, []));

  graphData.edges.forEach(edge => {
    if (blockedRoads.has(edge.id)) return;
    adj.get(edge.from).push(edge);
    if (!edge.isOneWay) {
      adj.get(edge.to).push({ ...edge, from: edge.to, to: edge.from });
    }
  });

  const targetNode = nodeMap.get(targetId);
  const heuristic = (id) => {
    if (algo === 'dijkstra') return 0;
    const n = nodeMap.get(id);
    return Math.hypot(n.x - targetNode.x, n.y - targetNode.y);
  };

  const gScore = new Map();
  const fScore = new Map();
  const cameFrom = new Map();
  const cameRoad = new Map();
  const distAccum = new Map();
  const timeAccum = new Map();

  graphData.nodes.forEach(n => {
    gScore.set(n.id, Infinity);
    fScore.set(n.id, Infinity);
  });

  gScore.set(startId, 0);
  fScore.set(startId, heuristic(startId));

  const openSet = [startId];
  let explored = 0;
  let found = false;

  while (openSet.length > 0) {
    openSet.sort((a, b) => fScore.get(a) - fScore.get(b));
    const curr = openSet.shift();
    explored++;

    if (curr === targetId) {
      found = true;
      break;
    }

    const neighbors = adj.get(curr) || [];
    for (const edge of neighbors) {
      const cost = (metric === 'time')
        ? (edge.distance / (edge.speedLimit * 1000 / 3600))
        : edge.distance;

      const tentativeG = gScore.get(curr) + cost;
      if (tentativeG < gScore.get(edge.to)) {
        cameFrom.set(edge.to, curr);
        cameRoad.set(edge.to, edge.id);
        distAccum.set(edge.to, edge.distance);
        timeAccum.set(edge.to, edge.distance / (edge.speedLimit * 1000 / 3600));

        gScore.set(edge.to, tentativeG);
        fScore.set(edge.to, tentativeG + heuristic(edge.to));

        if (!openSet.includes(edge.to)) {
          openSet.push(edge.to);
        }
      }
    }
  }

  const t1 = performance.now();
  if (!found) {
    return {
      success: false,
      statusMessage: 'No route found. Try clearing active road closures.',
      nodesExplored: explored,
      executionMicroseconds: Math.round((t1 - t0) * 1000)
    };
  }

  const pathNodes = [];
  const pathRoads = [];
  let curr = targetId;
  let totalDist = 0;
  let totalTime = 0;

  while (curr !== startId) {
    pathNodes.push(curr);
    pathRoads.push(cameRoad.get(curr));
    totalDist += distAccum.get(curr);
    totalTime += timeAccum.get(curr);
    curr = cameFrom.get(curr);
  }
  pathNodes.push(startId);
  pathNodes.reverse();
  pathRoads.reverse();

  // Generate directions
  const directions = [];
  for (let i = 0; i < pathNodes.length; i++) {
    const n = nodeMap.get(pathNodes[i]);
    if (i === 0) {
      directions.push({
        step: 1,
        action: 'Depart',
        instruction: `Depart from ${n.name}`
      });
    } else if (i === pathNodes.length - 1) {
      directions.push({
        step: i + 1,
        action: 'Arrive',
        instruction: `Arrive at destination: ${n.name}`
      });
    } else {
      directions.push({
        step: i + 1,
        action: 'Continue Straight',
        instruction: `At ${n.name}, continue along route`
      });
    }
  }

  return {
    success: true,
    algorithm: algo === 'astar' ? 'A* (Euclidean)' : 'Dijkstra',
    start: startId,
    target: targetId,
    totalDistance: totalDist,
    totalTimeSeconds: totalTime,
    nodesExplored: explored,
    executionMicroseconds: Math.round((t1 - t0) * 1000),
    pathNodes: pathNodes,
    pathRoads: pathRoads,
    directions: directions
  };
}

// Pan and Zoom Implementation
function setupPanZoom() {
  const viewport = document.getElementById('svgViewport');
  const svg = document.getElementById('mapSvg');

  viewport.addEventListener('wheel', (e) => {
    e.preventDefault();
    const zoomFactor = e.deltaY < 0 ? 1.1 : 0.9;
    zoomLevel = Math.max(0.5, Math.min(3.0, zoomLevel * zoomFactor));
    applyTransform();
  });

  viewport.addEventListener('mousedown', (e) => {
    if (e.target.closest('.node-group') || e.target.closest('.map-edge')) return;
    isPanning = true;
    startPanX = e.clientX - panX;
    startPanY = e.clientY - panY;
  });

  window.addEventListener('mousemove', (e) => {
    if (!isPanning) return;
    panX = e.clientX - startPanX;
    panY = e.clientY - startPanY;
    applyTransform();
  });

  window.addEventListener('mouseup', () => {
    isPanning = false;
  });
}

function applyTransform() {
  const gLayers = ['gridLayer', 'edgesLayer', 'routeLayer', 'nodesLayer'];
  gLayers.forEach(id => {
    const el = document.getElementById(id);
    if (el) {
      el.setAttribute('transform', `translate(${panX}, ${panY}) scale(${zoomLevel})`);
    }
  });
}

function zoomIn() {
  zoomLevel = Math.min(3.0, zoomLevel * 1.2);
  applyTransform();
}

function zoomOut() {
  zoomLevel = Math.max(0.5, zoomLevel / 1.2);
  applyTransform();
}

function resetZoom() {
  zoomLevel = 1.0;
  panX = 0;
  panY = 0;
  applyTransform();
}

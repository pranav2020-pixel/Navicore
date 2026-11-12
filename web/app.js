/**
 * NaviCore Route Planner - Frontend Controller
 * Discovers maps from /api/maps (or maps.json when opened standalone)
 * and routes via the C++ engine with a matching client-side fallback.
 */

let availableMaps = [];
let activeMap = '';
let graphData = { nodes: [], edges: [] };
let closedRoads = new Set();
let selectedStart = '';
let selectedTarget = '';
let isServerConnected = false;
let currentRouteNodes = new Set();

// Pan & Zoom state
let zoomLevel = 1.0;
let panX = 0;
let panY = 0;
let isPanning = false;
let startPanX = 0;
let startPanY = 0;

document.addEventListener('DOMContentLoaded', async () => {
  setupMobileMenu();
  setupPanZoom();
  await initEngineStatus();
  await loadAvailableMaps();
});

function setupMobileMenu() {
  const toggle = document.getElementById('menuToggle');
  const sidebar = document.getElementById('sidebar');
  if (!toggle || !sidebar) return;

  toggle.addEventListener('click', () => {
    const isOpen = sidebar.classList.toggle('is-open');
    toggle.setAttribute('aria-expanded', String(isOpen));
  });
}

async function initEngineStatus() {
  const label = document.getElementById('engineStatusLabel');
  const dot = document.querySelector('.status-dot');
  try {
    const res = await fetch('/api/status');
    if (res.ok) {
      isServerConnected = true;
      label.textContent = 'Using C++ engine';
      if (dot) dot.classList.remove('standalone');
      return;
    }
  } catch (_) {
    // Running statically without server.py
  }
  isServerConnected = false;
  label.textContent = 'Running in browser';
  if (dot) dot.classList.add('standalone');
}

async function loadAvailableMaps() {
  try {
    let res = isServerConnected ? await fetch('/api/maps') : null;
    if (!res || !res.ok) res = await fetch('maps.json');
    const data = await res.json();
    availableMaps = data.maps || [];
  } catch (err) {
    console.error('Could not load map list:', err);
    availableMaps = [];
  }

  renderMapTabs();
  if (availableMaps.length > 0) {
    await switchMap(availableMaps[0].id);
  }
}

function renderMapTabs() {
  const container = document.getElementById('mapTabs');
  container.innerHTML = '';

  availableMaps.forEach((m) => {
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = `map-tab ${m.id === activeMap ? 'active' : ''}`;
    btn.dataset.mapId = m.id;
    btn.innerHTML = `<span>${escapeHtml(m.label)}</span><span class="tab-count" id="tabCount-${escapeHtml(m.id)}"></span>`;
    btn.addEventListener('click', () => switchMap(m.id));
    container.appendChild(btn);
  });
}

async function switchMap(mapId) {
  if (activeMap === mapId) return;
  activeMap = mapId;

  document.querySelectorAll('.map-tab').forEach((tab) => {
    tab.classList.toggle('active', tab.dataset.mapId === mapId);
  });

  const info = availableMaps.find((m) => m.id === mapId);
  const titleEl = document.getElementById('sheetTitle');
  if (titleEl && info) {
    titleEl.textContent = `${info.label} route planner`;
  }

  closedRoads.clear();
  currentRouteNodes.clear();
  closeBenchmarkDrawer();
  hideNotice();
  resetZoom();

  await loadMapData(mapId);
  await calculateRoute();
}

async function loadMapData(mapId) {
  try {
    let res = isServerConnected ? await fetch(`/api/graph?map=${encodeURIComponent(mapId)}`) : null;
    if (!res || !res.ok) res = await fetch(`${mapId}_graph.json`);
    graphData = await res.json();

    const countEl = document.getElementById(`tabCount-${mapId}`);
    if (countEl) countEl.textContent = `${graphData.nodes.length} places`;

    fitViewBoxToNodes();
    populateDropdowns();
    renderMap();
    updateClosedRoadsUI();
  } catch (err) {
    showNotice('Could not load this map. Check that the map data file exists.');
    console.error('Failed to load map data:', err);
  }
}

// Fits the SVG coordinate frame to whatever node coordinates the loaded map uses.
function fitViewBoxToNodes() {
  const svg = document.getElementById('mapSvg');
  if (!svg || !graphData.nodes.length) return;

  let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
  graphData.nodes.forEach((n) => {
    if (n.x < minX) minX = n.x;
    if (n.y < minY) minY = n.y;
    if (n.x > maxX) maxX = n.x;
    if (n.y > maxY) maxY = n.y;
  });

  const padX = Math.max(90, (maxX - minX) * 0.1);
  const padY = Math.max(60, (maxY - minY) * 0.12);
  const width = Math.max(200, (maxX - minX) + padX * 2);
  const height = Math.max(200, (maxY - minY) + padY * 2);
  svg.setAttribute('viewBox', `${minX - padX} ${minY - padY} ${width} ${height}`);
}

// Picks a sensible initial pair (the two farthest places on the map) without hardcoding IDs.
function pickInitialPair(nodes) {
  if (nodes.length === 0) return ['', ''];
  if (nodes.length === 1) return [nodes[0].id, nodes[0].id];

  let bestA = nodes[0], bestB = nodes[nodes.length - 1], maxD = -1;
  for (let i = 0; i < nodes.length; i++) {
    for (let j = i + 1; j < nodes.length; j++) {
      const d = Math.hypot(nodes[i].x - nodes[j].x, nodes[i].y - nodes[j].y);
      if (d > maxD) {
        maxD = d;
        bestA = nodes[i];
        bestB = nodes[j];
      }
    }
  }
  return [bestA.id, bestB.id];
}

function populateDropdowns() {
  const startSelect = document.getElementById('selectStart');
  const targetSelect = document.getElementById('selectTarget');
  startSelect.innerHTML = '';
  targetSelect.innerHTML = '';

  const sorted = [...graphData.nodes].sort((a, b) => a.name.localeCompare(b.name));
  sorted.forEach((node) => {
    const label = `${node.name} (${node.id})`;
    startSelect.add(new Option(label, node.id));
    targetSelect.add(new Option(label, node.id));
  });

  const [defStart, defTarget] = pickInitialPair(graphData.nodes);
  startSelect.value = defStart;
  targetSelect.value = defTarget;
  selectedStart = defStart;
  selectedTarget = defTarget;

  startSelect.onchange = () => {
    selectedStart = startSelect.value;
    calculateRoute();
  };
  targetSelect.onchange = () => {
    selectedTarget = targetSelect.value;
    calculateRoute();
  };
  document.getElementById('selectAlgo').onchange = () => calculateRoute();
  document.getElementById('selectMetric').onchange = () => calculateRoute();
}

function renderMap() {
  const edgesLayer = document.getElementById('edgesLayer');
  const nodesLayer = document.getElementById('nodesLayer');
  const routeLayer = document.getElementById('routeLayer');

  edgesLayer.innerHTML = '';
  nodesLayer.innerHTML = '';
  routeLayer.innerHTML = '';

  const nodeMap = new Map(graphData.nodes.map((n) => [n.id, n]));

  graphData.edges.forEach((edge) => {
    const u = nodeMap.get(edge.from);
    const v = nodeMap.get(edge.to);
    if (!u || !v) return;

    const line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
    line.setAttribute('x1', u.x);
    line.setAttribute('y1', u.y);
    line.setAttribute('x2', v.x);
    line.setAttribute('y2', v.y);
    line.setAttribute('class', `map-edge ${closedRoads.has(edge.id) ? 'blocked' : ''}`);
    line.setAttribute('id', `edge-${edge.id}`);
    line.addEventListener('click', () => toggleRoadClosed(edge.id));

    const title = document.createElementNS('http://www.w3.org/2000/svg', 'title');
    title.textContent = `${u.name} to ${v.name} (${formatDistance(edge.distance)}, ${edge.speedLimit} km/h). Click to close or reopen.`;
    line.appendChild(title);

    edgesLayer.appendChild(line);
  });

  graphData.nodes.forEach((node) => {
    const g = document.createElementNS('http://www.w3.org/2000/svg', 'g');
    g.setAttribute('class', 'node-group');
    g.setAttribute('id', `node-group-${node.id}`);
    g.setAttribute('transform', `translate(${node.x}, ${node.y})`);

    const ring = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
    ring.setAttribute('class', 'node-ring');
    ring.setAttribute('r', '13');

    const core = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
    core.setAttribute('class', 'node-core');
    core.setAttribute('r', '7');

    const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');
    label.setAttribute('class', 'node-label');
    label.setAttribute('y', '-13');
    label.textContent = node.name;

    const title = document.createElementNS('http://www.w3.org/2000/svg', 'title');
    title.textContent = `${node.name} (${node.category})`;

    g.appendChild(title);
    g.appendChild(ring);
    g.appendChild(core);
    g.appendChild(label);

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
    updateNodeHighlights();
  } else if (selectedStart && !selectedTarget) {
    if (selectedStart === nodeId) return;
    selectedTarget = nodeId;
    targetSelect.value = nodeId;
    calculateRoute();
  }
}

function updateNodeHighlights() {
  document.querySelectorAll('.node-group').forEach((g) => {
    g.classList.remove('is-start', 'is-target', 'is-on-route');
  });

  currentRouteNodes.forEach((id) => {
    const el = document.getElementById(`node-group-${id}`);
    if (el) el.classList.add('is-on-route');
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

function toggleRoadClosed(roadId) {
  if (closedRoads.has(roadId)) closedRoads.delete(roadId);
  else closedRoads.add(roadId);

  const edgeEl = document.getElementById(`edge-${roadId}`);
  if (edgeEl) edgeEl.classList.toggle('blocked', closedRoads.has(roadId));

  updateClosedRoadsUI();
  if (selectedStart && selectedTarget) calculateRoute();
}

function reopenAllRoads() {
  closedRoads.clear();
  document.querySelectorAll('.map-edge').forEach((e) => e.classList.remove('blocked'));
  updateClosedRoadsUI();
  if (selectedStart && selectedTarget) calculateRoute();
}

function updateClosedRoadsUI() {
  const badge = document.getElementById('hazardCountBadge');
  const container = document.getElementById('hazardListContainer');
  badge.textContent = `${closedRoads.size} closed`;
  badge.classList.toggle('has-closed', closedRoads.size > 0);

  if (closedRoads.size === 0) {
    container.innerHTML = '<p class="muted-note">All roads are open.</p>';
    return;
  }

  const nodeMap = new Map(graphData.nodes.map((n) => [n.id, n.name]));
  container.innerHTML = '';

  closedRoads.forEach((roadId) => {
    const edge = graphData.edges.find((e) => e.id === roadId);
    const label = edge
      ? `${nodeMap.get(edge.from) || edge.from} to ${nodeMap.get(edge.to) || edge.to}`
      : roadId;

    const item = document.createElement('div');
    item.className = 'hazard-item';
    item.innerHTML = `
      <span class="hazard-road-name" title="${escapeHtml(label)}">${escapeHtml(label)}</span>
      <button type="button" class="hazard-reopen" onclick="toggleRoadClosed('${escapeHtml(roadId)}')">Reopen</button>
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
          algo,
          metric,
          blockedRoads: Array.from(closedRoads),
        }),
      });
      if (resp.ok) routeResult = await resp.json();
    } catch (err) {
      console.warn('Backend unavailable, using browser solver:', err);
    }
  }

  if (!routeResult) {
    routeResult = solveRouteClient(selectedStart, selectedTarget, algo, metric);
  }

  displayRoute(routeResult);
}

function displayRoute(res) {
  const routeLayer = document.getElementById('routeLayer');
  const dirContainer = document.getElementById('directionsList');

  if (!res || !res.success) {
    routeLayer.innerHTML = '';
    currentRouteNodes.clear();
    updateNodeHighlights();
    document.getElementById('hudDistance').textContent = '-';
    document.getElementById('hudTime').textContent = '-';
    document.getElementById('hudNodes').textContent = res ? `${res.nodesExplored} of ${graphData.nodes.length}` : '-';
    document.getElementById('hudLatency').textContent = '-';
    dirContainer.innerHTML = '<p class="muted-note">No open route connects these two places.</p>';
    showNotice('No open route connects these two places. Reopen a closed road to continue.');
    return;
  }

  hideNotice();

  document.getElementById('hudDistance').textContent = formatDistance(res.totalDistance);
  document.getElementById('hudTime').textContent = formatDuration(res.totalTimeSeconds);
  document.getElementById('hudNodes').textContent = `${res.nodesExplored} of ${graphData.nodes.length}`;
  document.getElementById('hudLatency').textContent = formatLatency(res.executionMicroseconds);

  const nodeMap = new Map(graphData.nodes.map((n) => [n.id, n]));
  const subtitle = document.getElementById('sheetSubtitle');
  const startName = nodeMap.get(res.start)?.name || res.start;
  const targetName = nodeMap.get(res.target)?.name || res.target;
  if (subtitle) {
    subtitle.textContent = `${startName} to ${targetName}`;
  }

  currentRouteNodes = new Set(res.pathNodes || []);
  updateNodeHighlights();

  routeLayer.innerHTML = '';
  if (res.pathNodes.length > 1) {
    let d = '';
    res.pathNodes.forEach((id, idx) => {
      const lm = nodeMap.get(id);
      if (lm) d += (idx === 0 ? `M ${lm.x} ${lm.y} ` : `L ${lm.x} ${lm.y} `);
    });
    const pathMain = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    pathMain.setAttribute('d', d);
    pathMain.setAttribute('class', 'route-path-main');
    routeLayer.appendChild(pathMain);
  }

  dirContainer.innerHTML = '';
  if (res.directions && res.directions.length > 0) {
    res.directions.forEach((step) => {
      const row = document.createElement('div');
      row.className = 'direction-step';
      const distLabel = step.segmentDistance > 0 ? formatDistance(step.segmentDistance) : '';
      row.innerHTML = `
        <span class="step-text">${step.step}. ${escapeHtml(step.instruction)}</span>
        ${distLabel ? `<span class="step-dist">${distLabel}</span>` : ''}
      `;
      dirContainer.appendChild(row);
    });
  }
}

function clearRouteDisplayOnly() {
  document.getElementById('routeLayer').innerHTML = '';
  currentRouteNodes.clear();
  document.getElementById('directionsList').innerHTML = '<p class="muted-note">Pick a destination on the map to see directions.</p>';
  document.getElementById('hudDistance').textContent = '-';
  document.getElementById('hudTime').textContent = '-';
  document.getElementById('hudNodes').textContent = '-';
  document.getElementById('hudLatency').textContent = '-';
  hideNotice();
}

function clearRoute() {
  reopenAllRoads();
  closeBenchmarkDrawer();
}

async function runBenchmark() {
  selectedStart = document.getElementById('selectStart').value;
  selectedTarget = document.getElementById('selectTarget').value;
  const metric = document.getElementById('selectMetric').value;
  if (!selectedStart || !selectedTarget) return;

  let resA = null, resD = null;
  if (isServerConnected) {
    try {
      const payload = (algo) => JSON.stringify({
        map: activeMap,
        start: selectedStart,
        target: selectedTarget,
        algo,
        metric,
        blockedRoads: Array.from(closedRoads),
      });
      const [rA, rD] = await Promise.all([
        fetch('/api/route', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: payload('astar') }),
        fetch('/api/route', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: payload('dijkstra') }),
      ]);
      if (rA.ok && rD.ok) {
        resA = await rA.json();
        resD = await rD.json();
      }
    } catch (_) {
      // Fall back to client solver below
    }
  }

  if (!resA || !resD) {
    resA = solveRouteClient(selectedStart, selectedTarget, 'astar', metric);
    resD = solveRouteClient(selectedStart, selectedTarget, 'dijkstra', metric);
  }

  const drawer = document.getElementById('comparisonDrawer');
  const tbody = document.getElementById('benchmarkTable');
  const summary = document.getElementById('comparisonSummary');

  const rows = [
    { name: 'Smart search (A*)', res: resA },
    { name: 'Exhaustive (Dijkstra)', res: resD },
  ];

  tbody.innerHTML = rows.map(({ name, res }) => `
    <tr>
      <td>${name}</td>
      <td class="num">${res.nodesExplored}</td>
      <td class="num">${formatLatency(res.executionMicroseconds)}</td>
      <td class="num">${res.success ? formatDistance(res.totalDistance) : '-'}</td>
      <td class="num">${res.success ? res.pathNodes.length : '-'}</td>
    </tr>
  `).join('');

  if (resA.success && resD.success && resD.nodesExplored > 0) {
    const savedPct = Math.round(((resD.nodesExplored - resA.nodesExplored) / resD.nodesExplored) * 100);
    const sameDist = Math.abs(resA.totalDistance - resD.totalDistance) < 1;
    summary.textContent = savedPct > 0
      ? `A* checked ${savedPct}% fewer places than Dijkstra${sameDist ? ' and found the same shortest route.' : '.'}`
      : 'Both searches checked the same number of places for this route.';
  } else {
    summary.textContent = 'No open route connects these two places.';
  }

  drawer.hidden = false;
}

function closeBenchmarkDrawer() {
  document.getElementById('comparisonDrawer').hidden = true;
}

function showNotice(text) {
  const el = document.getElementById('routeNotice');
  document.getElementById('routeNoticeText').textContent = text;
  el.hidden = false;
}

function hideNotice() {
  document.getElementById('routeNotice').hidden = true;
}

// Client-side solver matching the C++ PathFinder & NavigationEngine logic
function solveRouteClient(startId, targetId, algo, metric) {
  const t0 = performance.now();
  const nodeMap = new Map(graphData.nodes.map((n) => [n.id, n]));
  const adj = new Map(graphData.nodes.map((n) => [n.id, []]));

  // Derive scale from the loaded map so the A* heuristic is always admissible.
  let minMetresPerUnit = Infinity;
  let maxSpeedKmh = 0;

  graphData.edges.forEach((edge) => {
    if (edge.speedLimit > maxSpeedKmh) maxSpeedKmh = edge.speedLimit;
    const a = nodeMap.get(edge.from);
    const b = nodeMap.get(edge.to);
    if (a && b) {
      const units = Math.hypot(a.x - b.x, a.y - b.y);
      if (units > 0) minMetresPerUnit = Math.min(minMetresPerUnit, edge.distance / units);
    }
    if (closedRoads.has(edge.id)) return;
    adj.get(edge.from)?.push(edge);
    if (!edge.isOneWay) adj.get(edge.to)?.push({ ...edge, from: edge.to, to: edge.from });
  });

  if (!Number.isFinite(minMetresPerUnit)) minMetresPerUnit = 0;
  const maxSpeedMs = (maxSpeedKmh * 1000) / 3600;
  const targetNode = nodeMap.get(targetId);

  const heuristic = (id) => {
    if (algo === 'dijkstra' || !targetNode) return 0;
    const n = nodeMap.get(id);
    const metres = Math.hypot(n.x - targetNode.x, n.y - targetNode.y) * minMetresPerUnit;
    return metric === 'time' ? (maxSpeedMs > 0 ? metres / maxSpeedMs : 0) : metres;
  };

  const gScore = new Map();
  const fScore = new Map();
  const cameFrom = new Map();
  const cameRoad = new Map();
  const distAccum = new Map();
  const timeAccum = new Map();

  graphData.nodes.forEach((n) => {
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
    for (const edge of (adj.get(curr) || [])) {
      const speedMs = edge.speedLimit > 0 ? (edge.speedLimit * 1000) / 3600 : 1.4;
      const cost = metric === 'time' ? edge.distance / speedMs : edge.distance;
      const tentativeG = gScore.get(curr) + cost;
      if (tentativeG < gScore.get(edge.to)) {
        cameFrom.set(edge.to, curr);
        cameRoad.set(edge.to, edge.id);
        distAccum.set(edge.to, edge.distance);
        timeAccum.set(edge.to, edge.distance / speedMs);
        gScore.set(edge.to, tentativeG);
        fScore.set(edge.to, tentativeG + heuristic(edge.to));
        if (!openSet.includes(edge.to)) openSet.push(edge.to);
      }
    }
  }

  const elapsedUs = Math.round((performance.now() - t0) * 1000);
  if (!found) {
    return {
      success: false,
      nodesExplored: explored,
      executionMicroseconds: elapsedUs,
      pathNodes: [],
      pathRoads: [],
      directions: [],
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

  const edgeLookup = new Map(graphData.edges.map((e) => [e.id, e]));
  const directions = buildTurnDirections(pathNodes, pathRoads, nodeMap, edgeLookup);

  return {
    success: true,
    algorithm: algo === 'astar' ? 'A*' : 'Dijkstra',
    start: startId,
    target: targetId,
    totalDistance: totalDist,
    totalTimeSeconds: totalTime,
    nodesExplored: explored,
    executionMicroseconds: elapsedUs,
    pathNodes,
    pathRoads,
    directions,
  };
}

function buildTurnDirections(pathNodes, pathRoads, nodeMap, edgeLookup) {
  const STRAIGHT_DEG = 25;
  const U_TURN_DEG = 155;
  const steps = [];

  if (pathNodes.length === 0) return steps;
  const startNode = nodeMap.get(pathNodes[0]);
  if (pathNodes.length === 1) {
    return [{ step: 1, instruction: `You are already at ${startNode.name}.`, segmentDistance: 0 }];
  }

  steps.push({
    step: 1,
    instruction: `Leave ${startNode.name} toward ${nodeMap.get(pathNodes[1]).name}`,
    segmentDistance: 0,
  });

  for (let i = 1; i + 1 < pathNodes.length; i++) {
    const prev = nodeMap.get(pathNodes[i - 1]);
    const curr = nodeMap.get(pathNodes[i]);
    const next = nodeMap.get(pathNodes[i + 1]);

    const v1x = curr.x - prev.x, v1y = curr.y - prev.y;
    const v2x = next.x - curr.x, v2y = next.y - curr.y;
    const angleDeg = Math.atan2(v1x * v2y - v1y * v2x, v1x * v2x + v1y * v2y) * (180 / Math.PI);

    let text;
    if (Math.abs(angleDeg) < STRAIGHT_DEG) text = `Go straight past ${curr.name} toward ${next.name}`;
    else if (Math.abs(angleDeg) > U_TURN_DEG) text = `Make a U-turn at ${curr.name} toward ${next.name}`;
    else if (angleDeg > 0) text = `Turn right at ${curr.name} toward ${next.name}`;
    else text = `Turn left at ${curr.name} toward ${next.name}`;

    const road = edgeLookup.get(pathRoads[i - 1]);
    steps.push({
      step: i + 1,
      instruction: text,
      segmentDistance: road ? road.distance : Math.hypot(v1x, v1y),
    });
  }

  const lastRoad = edgeLookup.get(pathRoads[pathRoads.length - 1]);
  const dest = nodeMap.get(pathNodes[pathNodes.length - 1]);
  steps.push({
    step: pathNodes.length,
    instruction: `Arrive at ${dest.name}`,
    segmentDistance: lastRoad ? lastRoad.distance : 0,
  });

  return steps;
}

// Formatting helpers
function formatDistance(metres) {
  if (metres >= 1000) return `${(metres / 1000).toFixed(2)} km`;
  return `${Math.round(metres)} m`;
}

function formatDuration(seconds) {
  const total = Math.round(seconds);
  if (total < 60) return `${total} s`;
  const mins = Math.floor(total / 60);
  const secs = total % 60;
  return secs > 0 ? `${mins} min ${secs} s` : `${mins} min`;
}

function formatLatency(us) {
  if (!us || us <= 0) return '< 1 µs';
  if (us >= 1000) return `${(us / 1000).toFixed(1)} ms`;
  return `${us} µs`;
}

function escapeHtml(str) {
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

// Pan and Zoom
function setupPanZoom() {
  const viewport = document.getElementById('svgViewport');
  viewport.addEventListener('wheel', (e) => {
    e.preventDefault();
    const factor = e.deltaY < 0 ? 1.1 : 0.9;
    zoomLevel = Math.max(0.5, Math.min(3.0, zoomLevel * factor));
    applyTransform();
  });

  viewport.addEventListener('mousedown', (e) => {
    if (e.target.closest('.node-group') || e.target.closest('.map-edge') || e.target.closest('.map-controls')) return;
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
  ['edgesLayer', 'routeLayer', 'nodesLayer'].forEach((id) => {
    const el = document.getElementById(id);
    if (el) el.setAttribute('transform', `translate(${panX}, ${panY}) scale(${zoomLevel})`);
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

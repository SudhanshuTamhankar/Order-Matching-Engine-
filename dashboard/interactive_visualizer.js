/**
 * ApexMatch Interactive Architecture Visualizer
 * Core Simulation Engine & UI Orchestrator
 */

// --- Global Application State ---
const App = {
  activeTab: 'story',
  activeScenario: 'limit_rest',
  currentStep: 0,
  isPlaying: false,
  timerHandle: null,
  speed: 1.0,

  // Story Mode Order Book Memory
  bids: [
    { price: 100.40, orders: [{ id: 101, qty: 50 }, { id: 102, qty: 30 }] },
    { price: 100.30, orders: [{ id: 103, qty: 100 }] },
    { price: 100.20, orders: [{ id: 104, qty: 75 }] }
  ],
  asks: [
    { price: 100.60, orders: [{ id: 201, qty: 40 }, { id: 202, qty: 60 }] },
    { price: 100.70, orders: [{ id: 203, qty: 120 }] },
    { price: 100.80, orders: [{ id: 204, qty: 85 }] }
  ],
  orderIndex: {},

  // Terminal & Market Tape
  trades: [],
  chartPrices: [100.45, 100.50, 100.48, 100.50],
  lastPrice: 100.50
};

// --- Preset Scenarios Definition ---
const STORY_SCENARIOS = {
  limit_rest: {
    title: "Case 01: Limit Order (Non-Crossing)",
    packet: { side: "BUY LIMIT", sideClass: "buy", id: "Order #301", details: "45 shares @ $100.5000" },
    steps: [
      {
        stage: 1,
        title: "Step 1: Zero-Allocation Memory Pool",
        heading: "Popping Order from Preallocated Pool",
        desc: "Order #301 acquires a preallocated pointer from the contiguous OrderPool free list in O(1) time. Zero OS malloc calls.",
        complexity: "O(1) Memory Pool",
        stateBadge: "Memory Allocated",
        action: () => {}
      },
      {
        stage: 2,
        title: "Step 2: Multi-Threaded Validation & Queue",
        heading: "Parallel Sanity Checks & Ingress Buffer",
        desc: "OpenMP worker verifies price bounds ($100.50 > 0) and volume (45 > 0). Pushes order pointer into Bounded Ingress Queue.",
        complexity: "O(1) Lock-Free Ingress",
        stateBadge: "Validated & Queued",
        action: () => {}
      },
      {
        stage: 3,
        title: "Step 3: Deterministic Price Check",
        heading: "Evaluating Top of Opposite Book",
        desc: "Matching core evaluates m_asks.begin() ($100.60). Since Bid $100.50 < Best Ask $100.60, no cross occurs.",
        complexity: "O(1) map.begin() Access",
        stateBadge: "No Cross (Bid < Ask)",
        action: () => {}
      },
      {
        stage: 3,
        title: "Step 4: Insertion into Bid Book & Hash Index",
        heading: "Placing Order on Price Level List",
        desc: "Order #301 is appended to the doubly-linked list at $100.50. Global Hash Index records iterator for O(1) direct cancellation.",
        complexity: "O(log M) Tree + O(1) List",
        stateBadge: "Resting on Book",
        action: (app) => {
          app.bids.unshift({ price: 100.50, orders: [{ id: 301, qty: 45 }] });
          app.orderIndex[301] = { side: 'BUY', price: 100.50 };
        }
      },
      {
        stage: 4,
        title: "Step 5: Telemetry Snapshot Emitted",
        heading: "Asynchronous Handoff to Ring Buffer",
        desc: "L2 Depth snapshot emitted to lock-free SPSC Ring Buffer. Order is now active and resting on the book.",
        complexity: "O(1) SPSC Ring Buffer",
        stateBadge: "Active Resting",
        action: () => {}
      }
    ]
  },

  market_sweep: {
    title: "Case 02: Market Order (Aggressive Cross)",
    packet: { side: "MARKET BUY", sideClass: "buy", id: "Order #401", details: "70 shares @ Market" },
    steps: [
      {
        stage: 1,
        title: "Step 1: Rapid Ingress & Sanitization",
        heading: "Aggressive Buy Order Ingestion",
        desc: "Market Order for 70 shares arrives. Market orders execute immediately against the best available asks on the book.",
        complexity: "O(1) Ingress",
        stateBadge: "Incoming Market Taker",
        action: () => {}
      },
      {
        stage: 3,
        title: "Step 2: Best Ask Extraction",
        heading: "Targeting Top Price Level ($100.60)",
        desc: "Core matcher checks Lowest Ask Level ($100.60). Order #201 is at the front of the FIFO list with 40 shares.",
        complexity: "O(1) map.begin()",
        stateBadge: "Evaluating Best Ask",
        action: () => {}
      },
      {
        stage: 3,
        title: "Step 3: Trade Match 1 (Order #201 Fully Filled)",
        heading: "Executing 40 shares @ $100.60",
        desc: "Order #201 (40 shares) is matched, filled, and popped. Remaining taker quantity = 30 shares.",
        complexity: "O(1) FIFO Front Match",
        stateBadge: "40 Shares Traded",
        action: (app) => {
          app.asks[0].orders.shift();
          delete app.orderIndex[201];
          app.trades.unshift({ taker: 'BUY', price: 100.60, qty: 40, maker: 201, taker_id: 401 });
          app.lastPrice = 100.60;
          app.chartPrices.push(100.60);
        }
      },
      {
        stage: 3,
        title: "Step 4: Trade Match 2 (Partial Fill on Order #202)",
        heading: "Consuming 30 shares from Order #202",
        desc: "Taker consumes remaining 30 shares from Order #202 (Qty 60 -> 30). Taker order is now 100% fulfilled.",
        complexity: "O(1) Partial Match",
        stateBadge: "30 Shares Traded (Filled)",
        action: (app) => {
          app.asks[0].orders[0].qty = 30;
          app.trades.unshift({ taker: 'BUY', price: 100.60, qty: 30, maker: 202, taker_id: 401 });
        }
      },
      {
        stage: 4,
        title: "Step 5: Lock-Free Telemetry Broadcast",
        heading: "Trades Streamed to Async Logger",
        desc: "Executed matches are pushed across cache-line aligned SpscRingBuffers directly to persistence workers without blocking matching.",
        complexity: "O(1) SPSC Telemetry",
        stateBadge: "Trades Broadcasted",
        action: () => {}
      }
    ]
  },

  ioc_partial: {
    title: "Case 03: Immediate-Or-Cancel (IOC)",
    packet: { side: "IOC BUY", sideClass: "buy", id: "Order #501", details: "80 shares @ Limit $100.60" },
    steps: [
      {
        stage: 1,
        title: "Step 1: Immediate-Or-Cancel Ingress",
        heading: "IOC Order Ingestion",
        desc: "IOC BUY arrives for 80 shares @ Limit $100.60. An IOC order must trade immediately; unfulfilled shares are never placed on the book.",
        complexity: "O(1) Allocation",
        stateBadge: "IOC Ingress",
        action: () => {}
      },
      {
        stage: 3,
        title: "Step 2: Immediate Liquidity Match",
        heading: "Matching 40 shares @ $100.60",
        desc: "Matches against resting Ask #201 (40 shares). Executed Trade: 40 shares @ $100.60.",
        complexity: "O(1) Match",
        stateBadge: "40 Shares Executed",
        action: (app) => {
          app.asks[0].orders.shift();
          delete app.orderIndex[201];
          app.trades.unshift({ taker: 'BUY', price: 100.60, qty: 40, maker: 201, taker_id: 501 });
          app.lastPrice = 100.60;
          app.chartPrices.push(100.60);
        }
      },
      {
        stage: 3,
        title: "Step 3: Price Limit Reached (Trigger Cancel)",
        heading: "Cancelling Unfulfilled 40 Shares",
        desc: "Next available ask is $100.70 (> limit $100.60). Per IOC semantics, the remaining 40 shares are instantly discarded.",
        complexity: "O(1) IOC Cancellation",
        stateBadge: "Remainder Cancelled",
        action: () => {}
      },
      {
        stage: 4,
        title: "Step 4: Memory Recycled to Pool",
        heading: "Zero Resting Residual Volume",
        desc: "Order pointer is returned to OrderPool free list stack. The order book depth remains clean.",
        complexity: "O(1) Pool Return",
        stateBadge: "Memory Recycled",
        action: () => {}
      }
    ]
  },

  hash_cancel: {
    title: "Case 04: Hash Index Cancellation",
    packet: { side: "CANCEL", sideClass: "sell", id: "Target #202", details: "Target: Order #202 @ $100.60" },
    steps: [
      {
        stage: 1,
        title: "Step 1: Admin Cancel Request Arrives",
        heading: "Targeting Order #202",
        desc: "A cancellation command arrives targeting Order #202 (which is resting in the middle of the $100.60 queue).",
        complexity: "O(1) Ingress",
        stateBadge: "Cancel Target #202",
        action: () => {}
      },
      {
        stage: 3,
        title: "Step 2: Instant Hash Map Lookup",
        heading: "Extracting Iterator in O(1) Time",
        desc: "m_order_index.find(202) pulls the exact Side, Price Level, and direct std::list memory iterator in O(1) constant time.",
        complexity: "O(1) Hash Map",
        stateBadge: "Iterator Extracted",
        action: () => {}
      },
      {
        stage: 3,
        title: "Step 3: Direct Splicing from Linked List",
        heading: "Erasing Node Without Tree Traversal",
        desc: "Executing orders.erase(iterator) removes Order #202 directly from the doubly linked list in O(1) time without scanning any other orders.",
        complexity: "O(1) List Erase",
        stateBadge: "Node Spliced Out",
        action: (app) => {
          const askLvl = app.asks.find(l => l.price === 100.60);
          if (askLvl) {
            askLvl.orders = askLvl.orders.filter(o => o.id !== 202);
          }
          delete app.orderIndex[202];
        }
      },
      {
        stage: 4,
        title: "Step 4: Memory Returned & Index Pruned",
        heading: "Cancellation Complete",
        desc: "Target memory recycled to pool. Demonstrates the structural advantage of inverted hash indexing.",
        complexity: "O(1) Complete",
        stateBadge: "Order Cancelled",
        action: () => {}
      }
    ]
  }
};

// --- Initialization & State Reset ---
function resetStoryState() {
  App.currentStep = 0;
  App.isPlaying = false;
  clearInterval(App.timerHandle);
  document.getElementById('btn-story-play').textContent = 'Auto Play';

  App.bids = [
    { price: 100.40, orders: [{ id: 101, qty: 50 }, { id: 102, qty: 30 }] },
    { price: 100.30, orders: [{ id: 103, qty: 100 }] },
    { price: 100.20, orders: [{ id: 104, qty: 75 }] }
  ];
  App.asks = [
    { price: 100.60, orders: [{ id: 201, qty: 40 }, { id: 202, qty: 60 }] },
    { price: 100.70, orders: [{ id: 203, qty: 120 }] },
    { price: 100.80, orders: [{ id: 204, qty: 85 }] }
  ];
  App.trades = [
    { taker: 'BUY', price: 100.50, qty: 50, maker: 199, taker_id: 299 }
  ];
  App.chartPrices = [100.45, 100.50, 100.48, 100.50];
  App.lastPrice = 100.50;

  App.orderIndex = {};
  App.bids.forEach(l => l.orders.forEach(o => App.orderIndex[o.id] = { side: 'BUY', price: l.price }));
  App.asks.forEach(l => l.orders.forEach(o => App.orderIndex[o.id] = { side: 'SELL', price: l.price }));

  renderStoryUI();
}

// --- Story Stepping Controls ---
function storyStepNext() {
  const scenario = STORY_SCENARIOS[App.activeScenario];
  if (!scenario || App.currentStep >= scenario.steps.length) {
    pauseStory();
    return;
  }

  const step = scenario.steps[App.currentStep];
  step.action(App);
  App.currentStep++;
  renderStoryUI(step);

  if (App.currentStep >= scenario.steps.length) {
    pauseStory();
  }
}

function storyStepPrev() {
  if (App.currentStep <= 0) return;
  const targetStep = App.currentStep - 1;
  resetStoryState();
  const scenario = STORY_SCENARIOS[App.activeScenario];
  for (let i = 0; i < targetStep; i++) {
    scenario.steps[i].action(App);
    App.currentStep++;
  }
  const lastStep = targetStep > 0 ? scenario.steps[targetStep - 1] : null;
  renderStoryUI(lastStep);
}

function toggleStoryPlay() {
  if (App.isPlaying) {
    pauseStory();
  } else {
    playStory();
  }
}

function playStory() {
  App.isPlaying = true;
  document.getElementById('btn-story-play').textContent = 'Pause';
  const intervalMs = Math.max(300, 1500 / App.speed);
  App.timerHandle = setInterval(() => {
    const scenario = STORY_SCENARIOS[App.activeScenario];
    if (App.currentStep >= scenario.steps.length) {
      pauseStory();
      return;
    }
    storyStepNext();
  }, intervalMs);
}

function pauseStory() {
  App.isPlaying = false;
  clearInterval(App.timerHandle);
  document.getElementById('btn-story-play').textContent = 'Auto Play';
}

function selectScenario(key) {
  App.activeScenario = key;
  document.querySelectorAll('.scenario-card').forEach(card => {
    card.classList.toggle('active', card.dataset.scenario === key);
  });
  resetStoryState();
}

// --- Render Story UI ---
function renderStoryUI(activeStep = null) {
  const scenario = STORY_SCENARIOS[App.activeScenario];
  const totalSteps = scenario.steps.length;
  const curIdx = Math.max(1, App.currentStep);

  // Update Header & Badge
  document.getElementById('story-step-badge').textContent = `Step ${curIdx} of ${totalSteps}`;
  document.getElementById('story-step-title').textContent = activeStep ? activeStep.title : scenario.title;

  // Update Pipeline Track (Nodes & Lines)
  const activeStage = activeStep ? activeStep.stage : 1;
  for (let i = 1; i <= 4; i++) {
    const node = document.getElementById(`node-stage-${i}`);
    if (node) node.classList.toggle('active', i === activeStage);
  }
  document.getElementById('line-1-2').classList.toggle('filled', activeStage >= 2);
  document.getElementById('line-2-3').classList.toggle('filled', activeStage >= 3);
  document.getElementById('line-3-4').classList.toggle('filled', activeStage >= 4);

  // Update Order Packet
  const pktTag = document.getElementById('packet-side-tag');
  pktTag.textContent = scenario.packet.side;
  pktTag.className = `packet-tag ${scenario.packet.sideClass}`;
  document.getElementById('packet-id-text').textContent = scenario.packet.id;
  document.getElementById('packet-details-text').textContent = scenario.packet.details;
  document.getElementById('packet-state-text').textContent = activeStep ? activeStep.stateBadge : "Ready";

  // Update Narrative Box
  if (activeStep) {
    document.getElementById('narrative-heading').textContent = activeStep.heading;
    document.getElementById('narrative-desc').textContent = activeStep.desc;
    document.getElementById('narrative-complexity').textContent = activeStep.complexity;
  } else {
    document.getElementById('narrative-heading').textContent = "Execution State: Idle";
    document.getElementById('narrative-desc').textContent = "Select Next Step or Auto Play to execute the state machine lifecycle.";
    document.getElementById('narrative-complexity').textContent = "Deterministic O(1)";
  }

  // Render Visual Order Book Bids
  const bidsList = document.getElementById('story-bids-list');
  bidsList.innerHTML = '';
  App.bids.forEach(lvl => {
    const box = document.createElement('div');
    box.className = 'level-box bid';
    const totalVol = lvl.orders.reduce((sum, o) => sum + o.qty, 0);
    box.innerHTML = `
      <div class="level-top-row">
        <span class="price">$${lvl.price.toFixed(4)}</span>
        <span style="color:var(--text-muted)">Vol: ${totalVol}</span>
      </div>
      <div class="order-pills-row">
        ${lvl.orders.map(o => `<span class="order-pill">#${o.id} (${o.qty})</span>`).join('')}
      </div>
    `;
    bidsList.appendChild(box);
  });

  // Render Visual Order Book Asks
  const asksList = document.getElementById('story-asks-list');
  asksList.innerHTML = '';
  App.asks.forEach(lvl => {
    const box = document.createElement('div');
    box.className = 'level-box ask';
    const totalVol = lvl.orders.reduce((sum, o) => sum + o.qty, 0);
    box.innerHTML = `
      <div class="level-top-row">
        <span class="price">$${lvl.price.toFixed(4)}</span>
        <span style="color:var(--text-muted)">Vol: ${totalVol}</span>
      </div>
      <div class="order-pills-row">
        ${lvl.orders.map(o => `<span class="order-pill">#${o.id} (${o.qty})</span>`).join('')}
      </div>
    `;
    asksList.appendChild(box);
  });

  // Render Inverted Hash Table
  const hashGrid = document.getElementById('story-hash-grid');
  hashGrid.innerHTML = '';
  const entries = Object.entries(App.orderIndex).slice(0, 5);
  entries.forEach(([id, loc]) => {
    const item = document.createElement('div');
    item.className = 'hash-item';
    item.innerHTML = `
      <span class="key">Order #${id}</span>
      <span class="ptr">${loc.side} @ $${loc.price.toFixed(4)} ➔ std::list::iterator</span>
    `;
    hashGrid.appendChild(item);
  });
}

// --- Render Live Terminal Tab ---
function renderTerminalTab() {
  const tbody = document.getElementById('terminal-dom-tbody');
  if (!tbody) return;
  tbody.innerHTML = '';

  const maxVol = 200;

  // Asks
  [...App.asks].reverse().forEach(lvl => {
    const vol = lvl.orders.reduce((sum, o) => sum + o.qty, 0);
    const pct = Math.min(100, (vol / maxVol) * 100);
    const tr = document.createElement('tr');
    tr.innerHTML = `
      <td style="color:var(--color-ask); font-weight:700">$${lvl.price.toFixed(4)}</td>
      <td>${vol}</td>
      <td>${lvl.orders.length}</td>
      <td class="dom-bar-cell"><div class="dom-depth-bar ask" style="width:${pct}%"></div></td>
    `;
    tbody.appendChild(tr);
  });

  // Spread
  const bestBid = App.bids.length > 0 ? App.bids[0].price : 0;
  const bestAsk = App.asks.length > 0 ? App.asks[0].price : 0;
  const spread = (bestAsk > 0 && bestBid > 0) ? (bestAsk - bestBid).toFixed(4) : '0.0000';
  const spreadTr = document.createElement('tr');
  spreadTr.innerHTML = `<td colspan="4" style="text-align:center; padding:0.35rem; background:rgba(255,255,255,0.02); color:var(--text-dim); font-size:0.7rem; font-family:var(--font-mono)">SPREAD: $${spread}</td>`;
  tbody.appendChild(spreadTr);

  // Bids
  App.bids.forEach(lvl => {
    const vol = lvl.orders.reduce((sum, o) => sum + o.qty, 0);
    const pct = Math.min(100, (vol / maxVol) * 100);
    const tr = document.createElement('tr');
    tr.innerHTML = `
      <td style="color:var(--color-bid); font-weight:700">$${lvl.price.toFixed(4)}</td>
      <td>${vol}</td>
      <td>${lvl.orders.length}</td>
      <td class="dom-bar-cell"><div class="dom-depth-bar bid" style="width:${pct}%"></div></td>
    `;
    tbody.appendChild(tr);
  });

  // Tape
  const tapeList = document.getElementById('terminal-tape-list');
  if (tapeList) {
    tapeList.innerHTML = '';
    App.trades.slice(0, 7).forEach(t => {
      const li = document.createElement('li');
      li.className = 'modern-tape-item';
      li.innerHTML = `
        <span style="color:${t.taker === 'BUY' ? 'var(--color-bid)' : 'var(--color-ask)'}; font-weight:bold">${t.taker}</span>
        <span>$${t.price.toFixed(4)}</span>
        <span style="color:var(--text-muted)">Qty: ${t.qty}</span>
        <span style="color:var(--text-dim)">#${t.maker}x#${t.taker_id}</span>
      `;
      tapeList.appendChild(li);
    });
  }

  // Chart
  renderTerminalChart();
}

function renderTerminalChart() {
  const canvas = document.getElementById('terminal-chart');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  const w = canvas.width;
  const h = canvas.height;

  ctx.clearRect(0, 0, w, h);

  // Grid
  ctx.strokeStyle = '#1e293b';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(0, h/2); ctx.lineTo(w, h/2);
  ctx.stroke();

  if (App.chartPrices.length < 2) return;

  const minP = 100.20;
  const maxP = 100.80;

  ctx.strokeStyle = '#0ea5e9';
  ctx.lineWidth = 2;
  ctx.beginPath();

  App.chartPrices.forEach((p, idx) => {
    const x = (idx / (App.chartPrices.length - 1)) * (w - 15) + 8;
    const y = h - ((p - minP) / (maxP - minP)) * (h - 20) - 10;
    if (idx === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });

  ctx.stroke();
}

// --- 1M Stress Benchmark Runner ---
function runFullBenchmark() {
  const btn = document.getElementById('btn-run-full-bench');
  btn.disabled = true;
  btn.textContent = 'Simulating 1,000,000 Orders in In-Memory Core...';

  const startTime = performance.now();
  let count = 0;
  const total = 1000000;

  const interval = setInterval(() => {
    count += 100000;
    const elapsedSec = (performance.now() - startTime) / 1000;
    const tps = Math.floor(count / elapsedSec);

    document.getElementById('metric-tps').innerHTML = `${tps.toLocaleString()} <span class="unit">orders/sec</span>`;

    if (count >= total) {
      clearInterval(interval);
      btn.disabled = false;
      btn.textContent = 'Execute 1,000,000 Order Benchmark';
    }
  }, 70);
}

// --- Tab Switching Navigation ---
function switchTab(tabId) {
  App.activeTab = tabId;
  document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.tab === tabId);
  });
  document.querySelectorAll('.tab-content').forEach(content => {
    content.classList.toggle('active', content.id === `tab-${tabId}`);
  });

  if (tabId === 'terminal') {
    renderTerminalTab();
  }

  // Re-render math typography if KaTeX is loaded
  if (tabId === 'architecture' && window.renderMathInElement) {
    window.renderMathInElement(document.getElementById('tab-architecture'), {
      delimiters: [
        { left: '$$', right: '$$', display: true },
        { left: '$', right: '$', display: false }
      ]
    });
  }
}

// --- DOM Loaded Setup ---
document.addEventListener('DOMContentLoaded', () => {
  resetStoryState();

  // Tab buttons
  document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', () => switchTab(btn.dataset.tab));
  });

  // Scenario cards
  document.querySelectorAll('.scenario-card').forEach(card => {
    card.addEventListener('click', () => selectScenario(card.dataset.scenario));
  });

  // Story playback controls
  document.getElementById('btn-story-next').addEventListener('click', storyStepNext);
  document.getElementById('btn-story-prev').addEventListener('click', storyStepPrev);
  document.getElementById('btn-story-play').addEventListener('click', toggleStoryPlay);
  document.getElementById('btn-story-reset').addEventListener('click', resetStoryState);

  // Speed
  document.getElementById('story-speed').addEventListener('change', (e) => {
    App.speed = parseFloat(e.target.value);
    if (App.isPlaying) {
      pauseStory();
      playStory();
    }
  });

  // Benchmark button
  document.getElementById('btn-run-full-bench').addEventListener('click', runFullBenchmark);
});

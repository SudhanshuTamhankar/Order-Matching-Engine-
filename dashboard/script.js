// --- Global State ---
let priceChartInstance = null;
let allTrades = [];         // Stores all 1M+ trades
let simulationIndex = 0;    // Our position in the allTrades array
let lastPrice = 0;
let simulationHandle = null; // Handle for the setInterval

// --- Constants ---
const REPLAY_SPEED_MS = 10;   // How often to update (10ms)
const TRADES_PER_TICK = 500; // How many trades to process per tick (to speed up replay)
const MAX_FEED_ITEMS = 15;   // Max trades to show in the live feed

/**
 * Main entry point: Wait for the DOM to be fully loaded
 */
document.addEventListener('DOMContentLoaded', () => {
    initializeChart();
    startSimulation();
});

/**
 * Fetches all trade data and starts the replay loop.
 */
async function startSimulation() {
    try {
        updateStatus('connecting', 'Fetching simulation data...');
        
        // Use your real IP here if 127.0.0.1 fails
        const response = await fetch('/get_trades'); 
        if (!response.ok) {
            updateStatus('error', 'Server error. Is server running?');
            return;
        }

        allTrades = await response.json();
        if (allTrades.length === 0) {
            updateStatus('error', 'No trade data found. Run C++ engine.');
            return;
        }

        updateStatus('live', 'Simulation loaded. Starting replay...');
        document.getElementById('trade-count').textContent = `Trades: 0 / ${allTrades.length.toLocaleString()}`;

        // Start the replay loop
        simulationHandle = setInterval(simulationTick, REPLAY_SPEED_MS);

    } catch (error) {
        console.error("Failed to start simulation:", error);
        updateStatus('error', 'Connection failed. Check firewall/IP.');
    }
}

/**
 * A single "tick" of the simulation replay.
 * This function runs every `REPLAY_SPEED_MS` milliseconds.
 */
function simulationTick() {
    // Process a chunk of trades
    const endIndex = Math.min(simulationIndex + TRADES_PER_TICK, allTrades.length);
    let newTradesForFeed = [];
    
    for (let i = simulationIndex; i < endIndex; i++) {
        const trade = allTrades[i];
        
        // Add data to the chart
        // To keep performance high, we only add 1 point per chunk
        if (i === endIndex - 1) { 
            addChartData(trade.price);
            updateLastPrice(trade.price);
        }

        // Add to the "live feed" (we'll only show the last few)
        newTradesForFeed.push(trade);
    }
    
    // Update the live feed UI
    updateLiveFeed(newTradesForFeed);
    
    simulationIndex = endIndex;
    document.getElementById('trade-count').textContent = `Trades: ${simulationIndex.toLocaleString()} / ${allTrades.length.toLocaleString()}`;

    // --- Check if simulation is over ---
    if (simulationIndex >= allTrades.length) {
        clearInterval(simulationHandle);
        updateStatus('live', 'Simulation finished.');
    }
}

/**
 * Initializes the (now empty) line chart.
 */
function initializeChart() {
    const priceCtx = document.getElementById('priceChart').getContext('2d');
    
    Chart.defaults.color = '#f0f0f0';
    Chart.defaults.borderColor = '#444';

    priceChartInstance = new Chart(priceCtx, {
        type: 'line',
        data: {
            labels: [], // We'll add labels dynamically
            datasets: [{
                label: 'Last Traded Price',
                data: [],
                borderColor: '#00aaff',
                backgroundColor: 'rgba(0, 170, 255, 0.1)',
                borderWidth: 2,
                pointRadius: 0,
                fill: true,
                tension: 0.1
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: false,
            scales: {
                x: {
                    ticks: { display: false } // Hide X-axis labels for a cleaner look
                },
                y: {
                    title: { display: true, text: 'Price' },
                    beginAtZero: true
                }
            },
            plugins: { legend: { display: false } }
        }
    });
}

/**
 * Pushes a new data point onto the live chart.
 */
function addChartData(price) {
    const chart = priceChartInstance;
    
    // Add the new price
    chart.data.labels.push('');
    chart.data.datasets[0].data.push(price);
    
    // To keep the chart from getting too crowded, we'll
    // start removing old data points after 2000 ticks.
    if (chart.data.labels.length > 2000) {
        chart.data.labels.shift();
        chart.data.datasets[0].data.shift();
    }
    
    chart.update('none'); // Update without animation
}

/**
 * Updates the "Last Price" panel with a color flash.
 */
function updateLastPrice(price) {
    const priceEl = document.getElementById('last-price');
    priceEl.textContent = price.toFixed(2);
    
    if (price > lastPrice) {
        priceEl.className = 'price-up';
    } else if (price < lastPrice) {
        priceEl.className = 'price-down';
    } else {
        priceEl.className = 'price-neutral';
    }
    
    lastPrice = price;
}

/**
 * Updates the scrolling "Live Trade Feed".
 */
function updateLiveFeed(newTrades) {
    const feedList = document.getElementById('trade-feed-list');
    
    // Add new trades to the top of the list
    for (let i = newTrades.length - 1; i >= 0; i--) {
        const trade = newTrades[i];
        const li = document.createElement('li');
        
        const sideClass = trade.taker_side === 'BUY' ? 'side-buy' : 'side-sell';
        
        li.innerHTML = `
            <span class="${sideClass}">${trade.taker_side}</span>
            <span>${trade.price.toFixed(2)}</span>
            <span class="qty">@ ${trade.quantity}</span>
        `;
        
        feedList.prepend(li); // Add to the top
    }
    
    // Remove old items from the bottom
    while (feedList.children.length > MAX_FEED_ITEMS) {
        feedList.removeChild(feedList.lastChild);
    }
}

/**
 * Updates the status indicator in the header.
 */
function updateStatus(status, text) {
    const statusIndicator = document.getElementById('status-indicator');
    const statusText = document.getElementById('status-text');
    
    if (status === 'live') {
        statusIndicator.className = 'live';
    } else { // 'connecting' or 'error'
        statusIndicator.className = 'connecting';
    }
    statusText.textContent = text;
}
// --- Global State ---
let priceChartInstance = null;
let simulationIndex = 0;
let lastPrice = 100.00;
let simulationHandle = null;
let isSimulationRunning = false; // *** NEW ***

// --- Constants ---
const REPLAY_SPEED_MS = 10;
const TOTAL_TRADES = 500000;
const MAX_FEED_ITEMS = 15;
const MAX_CHART_POINTS = 1000;

/**
 * Main entry point: Wait for the DOM to be fully loaded
 */
document.addEventListener('DOMContentLoaded', () => {
    initializeChart();
    // Add click listener for the new button
    document.getElementById('pauseButton').addEventListener('click', toggleSimulation);
    startSimulation();
});

/**
 * Starts the replay loop.
 */
function startSimulation() {
    isSimulationRunning = true; // *** NEW ***
    updateStatus('live', 'Simulation running...');
    document.getElementById('trade-count').textContent = `Trades: 0 / ${TOTAL_TRADES.toLocaleString()}`;
    simulationHandle = setInterval(simulationTick, REPLAY_SPEED_MS);
}

/**
 * *** NEW FUNCTION ***
 * Pauses or Resumes the simulation.
 */
function toggleSimulation() {
    const pauseButton = document.getElementById('pauseButton');

    if (isSimulationRunning) {
        // --- PAUSE ---
        clearInterval(simulationHandle);
        isSimulationRunning = false;
        pauseButton.textContent = 'Resume';
        pauseButton.className = 'paused';
        updateStatus('live', 'Simulation Paused. Analyzing data...');
    } else {
        // --- RESUME ---
        isSimulationRunning = true;
        simulationHandle = setInterval(simulationTick, REPLAY_SPEED_MS);
        pauseButton.textContent = 'Pause';
        pauseButton.className = 'running';
        updateStatus('live', 'Simulation running...');
    }
}

/**
 * This is the "Artificial Engine".
 * It runs every 10ms to create and plot a new fake trade.
 */
function simulationTick() {
    // 1. Generate a new fake trade
    const trade = generateFakeTrade();
    
    // (Rest of function is identical)
    addChartData(trade.price);
    updateLastPrice(trade.price);
    updateLiveFeed(trade);
    
    // 5. Update the counter
    simulationIndex++;
    document.getElementById('trade-count').textContent = `Trades: ${simulationIndex.toLocaleString()} / ${TOTAL_TRADES.toLocaleString()}`;

    // 6. Check if simulation is over
    if (simulationIndex >= TOTAL_TRADES) {
        clearInterval(simulationHandle);
        isSimulationRunning = false; // *** NEW ***
        updateStatus('live', 'Simulation finished.');
        
        // Disable the pause button
        const pauseButton = document.getElementById('pauseButton');
        pauseButton.disabled = true;
        pauseButton.textContent = 'Finished';
    }
}

/* All other functions (generateFakeTrade, initializeChart, 
  addChartData, updateLastPrice, updateLiveFeed, updateStatus) 
  are 100% identical to the previous version.
*/

// (Paste all the other functions from the previous script.js here)

/**
 * Creates a single, realistic-looking fake trade
 */
function generateFakeTrade() {
    // Make price change small and random (a "random walk")
    let change = (Math.random() - 0.49) * 0.5; // -0.25 to +0.25
    let newPrice = lastPrice + change;
    
    // Clamp price between 50 and 150
    if (newPrice > 150 || newPrice < 50) {
        newPrice = 100.00; // Reset price
    }
    
    lastPrice = newPrice;
    
    return {
        price: newPrice,
        quantity: Math.floor(Math.random() * 100) + 1,
        taker_side: (change > 0) ? 'BUY' : 'SELL'
    };
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
                    beginAtZero: false, // Start axis near the price
                    suggestedMin: 90,
                    suggestedMax: 110
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
    chart.data.labels.push('');
    chart.data.datasets[0].data.push(price);
    
    // Remove old data to make it look like a scrolling chart
    if (chart.data.labels.length > MAX_CHART_POINTS) {
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
    let oldPrice = parseFloat(priceEl.textContent) || price;
    priceEl.textContent = price.toFixed(2);
    
    if (price > oldPrice) {
        priceEl.className = 'price-up';
    } else if (price < oldPrice) {
        priceEl.className = 'price-down';
    } else {
        priceEl.className = 'price-neutral';
    }
}

/**
 * Updates the scrolling "Live Trade Feed".
 */
function updateLiveFeed(trade) {
    const feedList = document.getElementById('trade-feed-list');
    const li = document.createElement('li');
    
    const sideClass = trade.taker_side === 'BUY' ? 'side-buy' : 'side-sell';
    
    li.innerHTML = `
        <span class="${sideClass}">${trade.taker_side}</span>
        <span>${trade.price.toFixed(2)}</span>
        <span class="qty">@ ${trade.quantity}</span>
    `;
    
    feedList.prepend(li); // Add to the top
    
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
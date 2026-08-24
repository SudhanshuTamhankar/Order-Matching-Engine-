import json
import os
from flask import Flask, send_from_directory, jsonify, send_file
from flask_cors import CORS

# Initialize the Flask app
app = Flask(__name__)

# --- Configuration ---
CORS(app)

TRADES_FILE_PATH = os.path.join(os.path.dirname(__file__), '../results/trades.json')
SNAPSHOTS_FILE_PATH = os.path.join(os.path.dirname(__file__), '../results/book_snapshots.jsonl')

# --- Static File Routes ---

@app.route('/')
def index():
    """Serves the main interactive_visualizer.html file or index.html."""
    if os.path.exists(os.path.join(os.path.dirname(__file__), 'interactive_visualizer.html')):
        return send_from_directory('.', 'interactive_visualizer.html')
    return send_from_directory('.', 'index.html')

@app.route('/classic')
def classic_index():
    return send_from_directory('.', 'index.html')

@app.route('/<path:filename>')
def static_files(filename):
    """Serves other static files (css, js)."""
    return send_from_directory('.', filename)

# --- API Routes ---

@app.route('/get_trades')
def get_trades():
    """API endpoint to read and return the contents of trades.json."""
    try:
        if not os.path.exists(TRADES_FILE_PATH):
            return jsonify([])
        return send_file(TRADES_FILE_PATH, mimetype='application/json')
    except Exception as e:
        print(f"Error reading trades file: {e}")
        return jsonify({"error": str(e)}), 500

@app.route('/get_snapshots')
def get_snapshots():
    """API endpoint to read and return book snapshots from book_snapshots.jsonl."""
    try:
        if not os.path.exists(SNAPSHOTS_FILE_PATH):
            return jsonify([])
        snapshots = []
        with open(SNAPSHOTS_FILE_PATH, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if line:
                    snapshots.append(json.loads(line))
        return jsonify(snapshots)
    except Exception as e:
        print(f"Error reading snapshots file: {e}")
        return jsonify({"error": str(e)}), 500

# --- Run the App ---

if __name__ == '__main__':
    print("Flask server running on http://127.0.0.1:5000")
    print(f"Serving trades from: {os.path.abspath(TRADES_FILE_PATH)}")
    print(f"Serving snapshots from: {os.path.abspath(SNAPSHOTS_FILE_PATH)}")
    app.run(debug=True, port=5000, host='0.0.0.0')
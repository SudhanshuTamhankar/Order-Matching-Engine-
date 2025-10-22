import os
from flask import Flask, send_from_directory, jsonify, send_file
from flask_cors import CORS

# Initialize the Flask app
app = Flask(__name__)

# --- Configuration ---

# Enable CORS (Cross-Origin Resource Sharing)
# This allows our script.js to make requests to our server,
# which is good practice.
CORS(app)

# Define the path to the trades.json file
# It's one level up from 'dashboard', in the 'results' folder
TRADES_FILE_PATH = os.path.join(os.path.dirname(__file__), '../results/trades.json')

# --- Static File Routes ---

@app.route('/')
def index():
    """Serves the main index.html file."""
    # send_from_directory is the safe way to serve files
    return send_from_directory('.', 'index.html')

@app.route('/<path:filename>')
def static_files(filename):
    """Serves other static files (css, js)."""
    return send_from_directory('.', filename)

# --- API Route ---

@app.route('/get_trades')
def get_trades():
    """
    API endpoint to read and return the contents of trades.json.
    """
    try:
        # Check if the file exists
        if not os.path.exists(TRADES_FILE_PATH):
            # If not (e.g., C++ hasn't run), return an empty list
            return jsonify([])
        
        # Send the file. Flask handles caching, etags, and mime types.
        return send_file(TRADES_FILE_PATH, mimetype='application/json')
    
    except Exception as e:
        # General error handling
        print(f"Error reading trades file: {e}")
        return jsonify({"error": str(e)}), 500

# --- Run the App ---

if __name__ == '__main__':
    """
    Runs the Flask server in debug mode.
    """
    print("Flask server running on http://127.0.0.1:5000")
    print(f"Serving trades from: {os.path.abspath(TRADES_FILE_PATH)}")
    # host='0.0.0.0' makes it accessible on your network (optional)
    app.run(debug=True, port=5000, host='0.0.0.0')
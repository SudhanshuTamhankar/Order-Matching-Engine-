from flask import Flask, send_from_directory

# Initialize the Flask app
app = Flask(__name__)

# Main route: serves index.html
@app.route('/')
def index():
    """Serves the main index.html file."""
    return send_from_directory('.', 'index.html')

# Static file route: serves styles.css, script.js
@app.route('/<path:filename>')
def static_files(filename):
    """Serves other static files."""
    return send_from_directory('.', filename)

# --- Run the App ---
if __name__ == '__main__':
    """
    Runs the Flask server.
    """
    print("--- Artificial Demo Server ---")
    print("This server fakes all the data and does not need the C++ engine.")
    print("Running at: http://127.0.0.1:4000/")
    app.run(port=4000)
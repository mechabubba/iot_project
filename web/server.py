from flask import Flask, request, jsonify, send_from_directory
from datetime import datetime

app = Flask(__name__)

receiver_data = []
beacon_data = [
    {"id": "beacon-1", "x": 0, "y": 0},
    {"id": "beacon-2", "x": 5, "y": 0},
    {"id": "beacon-3", "x": 0, "y": 5}
]

@app.route('/api/receiver', methods=['POST'])
def post_receiver_location():
    data = request.get_json()
    if not data or 'id' not in data or 'x' not in data or 'y' not in data:
        return jsonify({"error": "Missing fields"}), 400

    data['timestamp'] = datetime.utcnow().isoformat()
    receiver_data.append(data)
    return jsonify({"status": "Location received", "data": data}), 200

@app.route('/api/beacon', methods=['GET'])
def get_beacons():
    return jsonify(beacon_data), 200

@app.route('/api/receiver', methods=['GET'])
def get_receivers():
    return jsonify(receiver_data), 200


@app.route('/')
def serve_index():
    return send_from_directory('static', 'index.html')

@app.route('/api/config', methods=['POST'])
def update_config():
    global beacon_data
    data = request.get_json()
    beacon_data = data.get("beacons", beacon_data)
    with open("config.json", "w") as f:
        json.dump(data, f, indent=2)
    return jsonify({"status": "updated"})


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5050)


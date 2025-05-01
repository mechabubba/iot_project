#from flask import Flask, request, jsonify, send_from_directory
from flask import Flask, request, jsonify, send_from_directory, redirect, url_for, render_template, flash
from datetime import datetime
# Added the following imports
import sqlite3, json
# For png stuff for past sessions
import base64
import os
from werkzeug.security import generate_password_hash, check_password_hash

from flask_login import (
    LoginManager, UserMixin,
    login_user, login_required,
    logout_user, current_user, logout_user
)
from db_init import get_db_connection, initialize_database
##########################################
import json

from util import estimateDistance, estimateCoordinates

app = Flask(__name__)

# — Session & Login config —
app.secret_key = 'ReallyLongPassword69'

login_manager = LoginManager()
login_manager.login_view = 'login'
login_manager.init_app(app)

# Initialize your new Users table on startup
initialize_database()

receiver_data = []
beacon_data = [
    {"id": 0xA57A, "x": 0, "y": 0}, # green
    {"id": 0x9182, "x": 5, "y": 0}, # yellow
    {"id": 0x740A, "x": 0, "y": 5}, # red
    {"id": 0x93BE, "x": 5, "y": 5}  # blue
]

#Added the following class, definition and auth routes:
class User(UserMixin):
    def __init__(self, id, email, password_hash):
        self.id = id
        self.email = email
        self.password_hash = password_hash

@login_manager.user_loader
def load_user(user_id):
    conn = get_db_connection()
    row = conn.execute("SELECT * FROM Users WHERE id = ?", (user_id,)).fetchone()
    conn.close()
    if row:
        return User(row['id'], row['email'], row['password_hash'])
    return None

@app.route('/register', methods=['GET', 'POST'])
def register():
    if request.method == 'POST':
        email = request.form['email']
        pw_hash = generate_password_hash(request.form['password'])
        conn = get_db_connection()
        try:
            conn.execute(
              "INSERT INTO Users (email, password_hash) VALUES (?, ?)",
              (email, pw_hash)
            )
            conn.commit()
            flash("Registration successful. Please log in.", 'success')
            return redirect(url_for('login'))
        except sqlite3.IntegrityError:
            flash("That email’s already taken.", 'danger')
        finally:
            conn.close()
    return render_template('register.html')

@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        email = request.form['email']
        pw = request.form['password']
        conn = get_db_connection()
        row = conn.execute(
          "SELECT * FROM Users WHERE email = ?", (email,)
        ).fetchone()
        conn.close()
        if row and check_password_hash(row['password_hash'], pw):
            user = User(row['id'], row['email'], row['password_hash'])
            login_user(user)
            return redirect(url_for('index'))
        flash("Invalid credentials.", 'danger')
    return render_template('login.html')

@app.route('/logout')
@login_required
def logout():
    logout_user()
    return redirect(url_for('login'))

##########################################

@app.route('/api/receiver', methods=['POST'])
@login_required #                               
def post_receiver_location():
    data = request.get_json()
    if len(data) == 0:
        return jsonify({"error": f"Empty payload"}), 400
 
    for i in range(len(data)):
        if not data[i] or 'id' not in data[i] or 'rssi' not in data[i]:
            return jsonify({"error": f"Missing field in object {i}"}), 400

    # copy beacons list. filter them based on resulting values in receiver response.
    response_ids = {entry['id'] for entry in data}
    seen_beacons = [beacon for beacon in beacon_data if beacon['id'] in response_ids]

    dists = [estimateDistance(obj["rssi"]) for obj in data]
    coords = estimateCoordinates(seen_beacons, dists)

    loc = {}
    loc['x'] = coords[0].item()
    loc['y'] = coords[1].item()
    loc['timestamp'] = datetime.utcnow().isoformat()
    receiver_data.append(loc)
    return jsonify({"status": "Location received", "data": data}), 200

@app.route('/api/beacon', methods=['GET'])
@login_required #                               
def get_beacons():
    return jsonify(beacon_data), 200

@app.route('/api/receiver', methods=['GET'])
@login_required #                                
def get_receivers():
    return jsonify(receiver_data), 200


@app.route('/')
@login_required #                                
def index():
    return send_from_directory('templates', 'index.html')

@app.route('/api/config', methods=['POST'])
@login_required #                                
def update_config():
    global beacon_data
    data = request.get_json()
    beacon_data = data.get("beacons", beacon_data)
    with open("config.json", "w") as f:
        json.dump(data, f, indent=2)
    return jsonify({"status": "updated"})

@app.route('/api/heatmap')
def heatmap_data():
    session_id = request.args.get('sessionID')
    start_time = request.args.get('startTime')
    end_time = request.args.get('endTime')

    if not session_id:
        return jsonify({"error": "sessionID is required"}), 400

    conn = get_db_connection()
    query = 'SELECT X, Y, Timestamp FROM Position WHERE SessionID = ?'
    params = [session_id]

    if start_time and end_time:
        query += ' AND Timestamp BETWEEN ? AND ?'
        params += [start_time, end_time]

    rows = conn.execute(query, params).fetchall()
    conn.close()

    return jsonify([[row['X'], row['Y'], row['Timestamp']] for row in rows])

@app.route('/api/sessions')
def get_sessions():
    conn = get_db_connection()
    rows = conn.execute('''
        SELECT SessionID, StartTime, EndTime, Description, Width, Height, UserID
        FROM Session
        ORDER BY SessionID DESC
    ''').fetchall()
    conn.close()
    return jsonify([dict(row) for row in rows])

@app.route('/api/start_session', methods=['POST'])
def start_session():
    data = request.get_json()
    width = data.get("width")
    height = data.get("height")
    description = data.get("description", "Live session")

    if width is None or height is None:
        return jsonify({"error": "Width and Height required"}), 400
    
    user_id = current_user.id

    conn = get_db_connection()
    cursor = conn.execute("""
        INSERT INTO Session (StartTime, Description, Width, Height, UserID)
        VALUES (?, ?, ?, ?, ?)
    """, (datetime.now(), description, width, height, user_id))
    session_id = cursor.lastrowid
    conn.commit()
    conn.close()

    return jsonify({"sessionID": session_id, "userID": user_id})

@app.route('/api/end_session', methods=['POST'])
@login_required
def end_session():
    data = request.get_json()
    session_id = data.get("sessionID")

    if not session_id:
        return jsonify({"error": "sessionID is required"}), 400

    conn = get_db_connection()
    conn.execute("""
        UPDATE Session
        SET EndTime = ?
        WHERE SessionID = ? AND EndTime IS NULL
    """, (datetime.now(), session_id))
    conn.commit()
    conn.close()

    return jsonify({"status": "ended"})

#For session history saving the PNG 
@app.route('/api/save_room_drawing', methods=['POST'])
@login_required
def save_room_drawing():
    data = request.get_json()
    session_id = data.get('sessionID')
    drawing_data = data.get('drawingData')
    user_id = data.get('userID')

    if not session_id or not drawing_data:
        return jsonify({"error": "Missing sessionID or drawingData"}), 400

    header, encoded = drawing_data.split(",", 1)
    drawing_bytes = base64.b64decode(encoded)

    drawings_dir = "drawings"
    os.makedirs(drawings_dir, exist_ok=True)

    filepath = os.path.join(drawings_dir, f"drawing_session_{user_id}_{session_id}.png")
    with open(filepath, "wb") as f:
        f.write(drawing_bytes)

    return jsonify({"status": "saved", "file": filepath}), 200

@app.route('/drawings/<filename>')
@login_required
def get_drawing(filename):
    return send_from_directory('drawings', filename)

@app.route('/sessions')
@login_required
def sessions_page():
    return render_template('session.html')  

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5050)

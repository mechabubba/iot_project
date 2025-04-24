#from flask import Flask, request, jsonify, send_from_directory
from flask import Flask, request, jsonify, send_from_directory, redirect, url_for, render_template, flash
from datetime import datetime
# Added the following imports
import sqlite3, json
from werkzeug.security import generate_password_hash, check_password_hash

from flask_login import (
    LoginManager, UserMixin,
    login_user, login_required,
    logout_user, current_user, logout_user
)
from db_init import get_db_connection, initialize_database
##########################################
import json

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
    {"id": "beacon-1", "x": 0, "y": 0},
    {"id": "beacon-2", "x": 5, "y": 0},
    {"id": "beacon-3", "x": 0, "y": 5}
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
@login_required #                                <- Added this line.
def post_receiver_location():
    data = request.get_json()
    if not data or 'id' not in data or 'x' not in data or 'y' not in data:
        return jsonify({"error": "Missing fields"}), 400

    data['timestamp'] = datetime.utcnow().isoformat()
    receiver_data.append(data)
    return jsonify({"status": "Location received", "data": data}), 200

@app.route('/api/beacon', methods=['GET'])
@login_required #                                <- Added this line.
def get_beacons():
    return jsonify(beacon_data), 200

@app.route('/api/receiver', methods=['GET'])
@login_required #                                <- Added this line.
def get_receivers():
    return jsonify(receiver_data), 200


@app.route('/')
@login_required #                                <- Added this line.
def index():
    return send_from_directory('templates', 'index.html') #changed "static" to "templates".

@app.route('/api/config', methods=['POST'])
@login_required #                                <- Added this line.
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
    query = 'SELECT X, Y FROM Position WHERE SessionID = ?'
    params = [session_id]

    if start_time and end_time:
        query += ' AND Timestamp BETWEEN ? AND ?'
        params += [start_time, end_time]

    rows = conn.execute(query, params).fetchall()
    conn.close()

    return jsonify([[row['X'], row['Y']] for row in rows])

@app.route('/api/sessions')
def get_sessions():
    conn = get_db_connection()
    rows = conn.execute('''
        SELECT SessionID, StartTime, EndTime, Description, Width, Height
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

    conn = get_db_connection()
    cursor = conn.execute("""
        INSERT INTO Session (StartTime, Description, Width, Height)
        VALUES (?, ?, ?, ?)
    """, (datetime.now(), description, width, height))
    session_id = cursor.lastrowid
    conn.commit()
    conn.close()

    return jsonify({"sessionID": session_id})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5050)


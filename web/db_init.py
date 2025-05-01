import sqlite3
from datetime import datetime, timedelta
import random
from werkzeug.security import generate_password_hash, check_password_hash #Added

sqlite3.register_adapter(datetime, lambda dt: dt.isoformat())

sqlite3.register_converter("timestamp", lambda s: datetime.fromisoformat(s.decode()))

def get_db_connection():
    conn = conn = sqlite3.connect('cat_tracker.db', detect_types=sqlite3.PARSE_DECLTYPES)
    conn.execute('PRAGMA foreign_keys = ON;')
    conn.row_factory = sqlite3.Row
    return conn

#Added this table for users for auth.
def create_user_table():
    conn = get_db_connection()
    conn.execute("""
      CREATE TABLE IF NOT EXISTS Users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        email TEXT UNIQUE NOT NULL,
        password_hash TEXT NOT NULL,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
      )
    """)
    conn.close()

# Create Session table
def create_session_table():
    conn = get_db_connection()
    conn.execute("""
        CREATE TABLE IF NOT EXISTS Session (
        SessionID INTEGER PRIMARY KEY AUTOINCREMENT,
        StartTime timestamp NOT NULL,
        EndTime timestamp,
        Description TEXT,
        Width REAL NOT NULL,
        Height REAL NOT NULL
        );""")
    conn.close()

# Create Position table
def create_position_table():
    conn = get_db_connection()
    conn.execute("""
        CREATE TABLE IF NOT EXISTS Position (
        PositionID INTEGER PRIMARY KEY AUTOINCREMENT,
        SessionID INTEGER NOT NULL,
        Timestamp timestamp DEFAULT CURRENT_TIMESTAMP,
        X REAL NOT NULL,
        Y REAL NOT NULL,
        FOREIGN KEY (SessionID) REFERENCES Session(SessionID)
        );""")
    conn.close()

def initialize_database():
    create_session_table()
    create_position_table()
    create_user_table() #added
    print("Database Initialized")
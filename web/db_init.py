import sqlite3
from datetime import datetime, timedelta
import random

def get_db_connection():
    conn = conn = sqlite3.connect('cat_tracker.db')
    conn.execute('PRAGMA foreign_keys = ON;')
    conn.row_factory = sqlite3.Row
    return conn

# Create Session table
def create_session_table():
    conn = get_db_connection()
    conn.execute("""
        CREATE TABLE IF NOT EXISTS Session (
        SessionID INTEGER PRIMARY KEY AUTOINCREMENT,
        StartTime DATETIME NOT NULL,
        EndTime DATETIME,
        Description TEXT
        );""")
    conn.close()

# Create Position table
def create_position_table():
    conn = get_db_connection()
    conn.execute("""
        CREATE TABLE IF NOT EXISTS Position (
        PositionID INTEGER PRIMARY KEY AUTOINCREMENT,
        SessionID INTEGER NOT NULL,
        Timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
        X REAL NOT NULL,
        Y REAL NOT NULL,
        FOREIGN KEY (SessionID) REFERENCES Session(SessionID)
        );""")
    conn.close()

def initialize_database():
    create_session_table()
    create_position_table()
    print("Database Initialized")
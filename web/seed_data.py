from datetime import datetime, timedelta
from werkzeug.security import generate_password_hash
from db_init import get_db_connection

DB_PATH = "cat_tracker.db"

def seed_data():
    conn = get_db_connection()
    cursor = conn.cursor()

    # 1. Insert user
    email = "rock@gmail.com"
    password_hash = generate_password_hash("rock123")

    cursor.execute("""
        INSERT OR IGNORE INTO Users (email, password_hash)
        VALUES (?, ?)
    """, (email, password_hash))

    # 2. Insert session (10-minute session)
    start_time = datetime.now() - timedelta(days=1)
    end_time = start_time + timedelta(minutes=10)
    width, height = 10, 10
    description = "Seeded test session"

    cursor.execute("""
        INSERT INTO Session (StartTime, EndTime, Description, Width, Height, UserID)
        VALUES (?, ?, ?, ?, ?, ?)
    """, (start_time, end_time, description, width, height, 1))

    session_id = cursor.lastrowid

    # 3. Insert example position data (X, Y within 10x10)
    for i in range(20):  # 20 points spaced over 10 minutes
        x = round((i % 10) + 0.5, 2)
        y = round((i // 2) + 0.3, 2)
        timestamp = start_time + timedelta(seconds=30 * i)
        cursor.execute("""
            INSERT INTO Position (SessionID, Timestamp, X, Y)
            VALUES (?, ?, ?, ?)
        """, (session_id, timestamp, x, y))

    conn.commit()
    conn.close()
    print("Seeded user, session, and position data successfully.")

if __name__ == "__main__":
    seed_data()

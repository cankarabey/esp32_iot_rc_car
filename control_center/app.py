import json
from flask import Flask, jsonify, request, render_template

app = Flask(__name__)

motor_action = 0
steering_angle = 0
motor_speed = 0

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/update', methods=['POST'])
def update_values():
    global motor_action, steering_angle, motor_speed
    content = request.json
    motor_action = int(content.get('motorAction', motor_action))
    motor_speed = int(content.get('motorSpeed', motor_speed))
    steering_angle = int(content.get('steeringAngle', steering_angle)) + 90
    return jsonify({"motor_action": motor_action, "steering_angle": steering_angle, "motor_speed": motor_speed})

@app.route('/status', methods=['GET'])
def get_status():
    return jsonify({"motor_action": motor_action, "steering_angle": steering_angle, "motor_speed": motor_speed})

if __name__ == '__main__':
   app.run(port=5000, host="0.0.0.0")
#!/usr/bin/env python3
"""Pubblica cmd_motor_test a rate fisso attraversando una sequenza di duty
senza interruzioni: il watchdog da 500 ms di task_motors non scatta mai tra
un gradino e l'altro (con `ros2 topic pub` invece ogni cambio valore
richiede Ctrl+C + rilancio, > 500 ms, e i motori ripartono da fermo).

Uso:
    source /opt/ros/humble/setup.bash
    python3 motor_steps.py "10,10,10,0:3" "20,20,20,0:3" "30,30,30,0:5"

Ogni argomento e' "FL,RL,RR,FR:secondi". A fine sequenza, o a Ctrl+C,
pubblica [0,0,0,0] e chiude. Ordine motori come in cmd_motor_test.
"""

import argparse
import sys
import time

import rclpy
from rclpy.node import Node
from std_msgs.msg import Float32MultiArray


def parse_step(arg):
    duties, _, hold = arg.partition(":")
    vals = [float(v) for v in duties.split(",")]
    if len(vals) != 4:
        raise argparse.ArgumentTypeError(f"'{arg}': servono 4 duty, trovati {len(vals)}")
    if not all(0.0 <= v <= 100.0 for v in vals):
        raise argparse.ArgumentTypeError(f"'{arg}': duty fuori da 0-100")
    return vals, float(hold) if hold else 3.0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("steps", nargs="+", type=parse_step, help='"FL,RL,RR,FR:secondi"')
    ap.add_argument("--ns", default="drone_1", help="namespace ROS del drone (default drone_1)")
    ap.add_argument("--rate", type=float, default=10.0, help="Hz di pubblicazione (default 10, min 3 per il watchdog)")
    args = ap.parse_args()

    if args.rate < 3.0:
        sys.exit("rate < 3 Hz: il watchdog motori (500 ms) scatterebbe")

    rclpy.init()
    node = Node("motor_steps")
    pub = node.create_publisher(Float32MultiArray, f"/{args.ns}/cmd_motor_test", 1)
    period = 1.0 / args.rate
    msg = Float32MultiArray()

    # Aspetta che l'agent abbia agganciato il subscriber, altrimenti i primi
    # messaggi vanno persi e il primo gradino parte in ritardo.
    t0 = time.monotonic()
    while pub.get_subscription_count() == 0:
        if time.monotonic() - t0 > 5.0:
            sys.exit(f"nessun subscriber su /{args.ns}/cmd_motor_test: agent giu' o drone non connesso")
        time.sleep(0.1)

    def publish(vals):
        msg.data = vals
        pub.publish(msg)

    try:
        for vals, hold in args.steps:
            print(f"-> {vals} per {hold:.1f} s", flush=True)
            t_end = time.monotonic() + hold
            while time.monotonic() < t_end:
                publish(vals)
                time.sleep(period)
    except KeyboardInterrupt:
        print("\nCtrl+C", flush=True)
    finally:
        # Zero esplicito, ripetuto: non affidarsi al watchdog per fermarsi.
        for _ in range(3):
            publish([0.0, 0.0, 0.0, 0.0])
            time.sleep(period)
        print("-> [0,0,0,0]", flush=True)
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()

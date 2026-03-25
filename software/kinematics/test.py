import numpy as np
from ikpy.chain import Chain
from scipy.spatial.transform import Rotation as R

# Load URDF
robot_chain = Chain.from_urdf_file("../urdf/ns16.urdf")

# Target pose in meters
x, y, z = 0.2, 0.05, 0.15

# Target orientation as RPY in degrees
roll, pitch, yaw = 0, 90, 0
rot_matrix = R.from_euler('xyz', [roll, pitch, yaw], degrees=True).as_matrix()

# Build full 4x4 homogeneous transform
target_pose = np.eye(4)
target_pose[:3, :3] = rot_matrix
target_pose[:3, 3] = [x, y, z]

# Compute IK
# joint_angles = robot_chain.inverse_kinematics(target_pose)

# Print joint angles
#print("Joint angles (radians):", joint_angles)
#print("Joint angles (degrees):", np.degrees(joint_angles))

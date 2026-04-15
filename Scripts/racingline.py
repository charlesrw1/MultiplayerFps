import numpy as np
import matplotlib.pyplot as plt
from scipy.interpolate import splprep, splev
from scipy.ndimage import gaussian_filter1d

# -----------------------------
# Example track
# -----------------------------
t = np.linspace(0, 2*np.pi, 200)
x = t
y = np.sin(t)
track = np.vstack((x, y)).T

track_width = 1.0

# -----------------------------
# Normals (stable)
# -----------------------------
def compute_normals(path):
    dx = np.gradient(path[:, 0])
    dy = np.gradient(path[:, 1])
    
    length = np.sqrt(dx**2 + dy**2) + 1e-6
    tx = dx / length
    ty = dy / length
    
    nx = -ty
    ny = tx
    
    return np.vstack((nx, ny)).T

normals = compute_normals(track)

# -----------------------------
# Curvature (smoothed!)
# -----------------------------
def curvature(path):
    dx = np.gradient(path[:, 0])
    dy = np.gradient(path[:, 1])
    ddx = np.gradient(dx)
    ddy = np.gradient(dy)
    
    num = dx * ddy - dy * ddx
    denom = (dx**2 + dy**2)**1.5 + 1e-6
    return num / denom  # signed curvature

curv = curvature(track)

# smooth it → CRITICAL
curv = gaussian_filter1d(curv, sigma=5)

# -----------------------------
# Safe offset (bounded)
# -----------------------------
offset = -curv  # invert so we go "outside" of corner

# normalize
offset /= (np.max(np.abs(offset)) + 1e-6)

# clamp to track width
max_offset = track_width / 2 * 0.9
offset *= max_offset

# -----------------------------
# Apply offset
# -----------------------------
racing_line = track + normals * offset[:, None]

# -----------------------------
# Light smoothing ONLY
# -----------------------------
tck, _ = splprep([racing_line[:,0], racing_line[:,1]], s=0.1)
u = np.linspace(0, 1, len(track))
smooth = np.array(splev(u, tck)).T

# -----------------------------
# Plot
# -----------------------------
plt.figure(figsize=(10,5))
plt.plot(track[:,0], track[:,1], '--', label="Centerline")
plt.plot(smooth[:,0], smooth[:,1], label="Racing Line")
plt.axis('equal')
plt.legend()
plt.title("Stable Racing Line (No Loops)")
plt.show()
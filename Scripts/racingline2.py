import numpy as np
import matplotlib.pyplot as plt
from scipy.interpolate import splprep, splev
from scipy.ndimage import gaussian_filter1d

# -----------------------------
# PARAMETERS
# -----------------------------
track_width = 2.0
corner_radius = 3.0   # MUST be >= track_width/2 for clean geometry

# -----------------------------
# BUILD TRACK (straight → arc → straight)
# -----------------------------
# Straight 1
straight1 = np.array([[x, 0] for x in np.linspace(0, 10, 100)])

# Quarter circle (fillet corner)
theta = np.linspace(0, np.pi/2, 100)
corner = np.array([
    [10 + corner_radius*(np.cos(t)-1),
     corner_radius*np.sin(t)]
    for t in theta
])

# Straight 2 (vertical exit)
straight2 = np.array([
    [10 - corner_radius, y]
    for y in np.linspace(corner_radius, 10, 100)
])

# Combine track
track = np.vstack((straight1[:-1], corner[:-1], straight2))

# -----------------------------
# RESAMPLE EVENLY
# -----------------------------
def resample(path, n=400):
    dist = np.cumsum(np.sqrt(np.sum(np.diff(path, axis=0)**2, axis=1)))
    dist = np.insert(dist, 0, 0)
    uniform = np.linspace(0, dist[-1], n)

    x = np.interp(uniform, dist, path[:,0])
    y = np.interp(uniform, dist, path[:,1])

    return np.vstack((x, y)).T

track = resample(track)

# -----------------------------
# COMPUTE NORMALS
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
# CURVATURE (SMOOTHED)
# -----------------------------
def curvature(path):
    dx = np.gradient(path[:, 0])
    dy = np.gradient(path[:, 1])
    ddx = np.gradient(dx)
    ddy = np.gradient(dy)

    return (dx * ddy - dy * ddx) / ((dx**2 + dy**2)**1.5 + 1e-6)

curv = gaussian_filter1d(curvature(track), sigma=5)

# -----------------------------
# RACING LINE OFFSET
# -----------------------------
offset = -curv
offset /= (np.max(np.abs(offset)) + 1e-6)

max_offset = track_width / 2 * 0.95
offset *= max_offset

racing_line = track + normals * offset[:, None]

# -----------------------------
# SMOOTH RACING LINE
# -----------------------------
tck, _ = splprep([racing_line[:,0], racing_line[:,1]], s=0.2)
u = np.linspace(0, 1, len(track))
racing_smooth = np.array(splev(u, tck)).T

# -----------------------------
# TRACK BOUNDARIES
# -----------------------------
left_edge  = track + normals * (track_width / 2)
right_edge = track - normals * (track_width / 2)

# -----------------------------
# PLOT
# -----------------------------
plt.figure(figsize=(7,7))

# Track edges
plt.plot(left_edge[:,0], left_edge[:,1], 'k', linewidth=2)
plt.plot(right_edge[:,0], right_edge[:,1], 'k', linewidth=2)

# Fill track
plt.fill_betweenx(left_edge[:,1], left_edge[:,0], right_edge[:,0],
                  color='lightgray', alpha=0.3)

# Centerline
plt.plot(track[:,0], track[:,1], '--', color='gray', label="Centerline")

# Racing line
plt.plot(racing_smooth[:,0], racing_smooth[:,1], 'r', linewidth=2, label="Racing Line")

plt.axis('equal')
plt.title("90° Track with Proper Racing Line")
plt.legend()
plt.show()
import { CreateGL3, ANTIALIAS, RGBA } from './nanovg.js';

// 1. Locate or create the target canvas element in the webpage
const canvas = document.getElementById('canvas') || document.querySelector('canvas') || (() => {
  const c = document.createElement('canvas');
  c.id = 'canvas';
  document.body.appendChild(c);
  return c;
})();

// 2. Initialize the NanoVG WebGL rendering context
const nvg = CreateGL3(ANTIALIAS);

// 3. Main browser animation loop using requestAnimationFrame
function render() {
  const width = window.innerWidth;
  const height = window.innerHeight;

  // Sync canvas drawing buffer size with viewport dimensions
  if (canvas.width !== width || canvas.height !== height) {
    canvas.width = width;
    canvas.height = height;
  }

  const t = performance.now() / 1000.0;

  // Begin the NanoVG frame (width, height, pixel ratio for high-DPI/Retina displays)
  nvg.BeginFrame(width, height, window.devicePixelRatio || 1);

  // Draw Deep Space Background
  nvg.BeginPath();
  nvg.Rect(0, 0, width, height);
  nvg.FillColor(RGBA(10, 12, 22, 255));
  nvg.Fill();

  const cx = width / 2;
  const cy = height / 2;

  // Draw Orbit Guidelines
  const orbits = [130, 210, 300];
  orbits.forEach((radius, index) => {
    nvg.BeginPath();
    nvg.Circle(cx, cy, radius);
    nvg.StrokeColor(RGBA(255, 255, 255, 20 + index * 10));
    nvg.StrokeWidth(1.0);
    nvg.Stroke();
  });

  // Draw the Central Sun
  nvg.BeginPath();
  nvg.Circle(cx, cy, 45);
  nvg.FillColor(RGBA(255, 170, 30, 255));
  nvg.Fill();

  // Define Planets: [orbit distance, radius, orbital speed, color]
  const planets = [
    { dist: 130, radius: 10, speed: 1.4, color: RGBA(90, 170, 255, 255) }, // Inner Planet
    { dist: 210, radius: 15, speed: 0.9, color: RGBA(230, 80, 50, 255) },  // Mid Planet
    { dist: 300, radius: 12, speed: 0.5, color: RGBA(110, 230, 150, 255) } // Outer Planet
  ];

  // Render Each Orbiting Planet
  planets.forEach(p => {
    const angle = t * p.speed;
    const x = cx + Math.cos(angle) * p.dist;
    const y = cy + Math.sin(angle) * p.dist;

    nvg.BeginPath();
    nvg.Circle(x, y, p.radius);
    nvg.FillColor(p.color);
    nvg.Fill();
  });

  // Finalize frame rendering
  nvg.EndFrame();

  // Schedule the next frame
  requestAnimationFrame(render);
}

// Start the render loop
requestAnimationFrame(render);

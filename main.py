#!/usr/bin/env python3
"""
DOOM Dark Ages - WAD Loader Game
A Chocolate Doom-style game with Nokia1709 branding
"""

import pygame
import sys
import os
import math
import json
import struct
import numpy as np
from dataclasses import dataclass
from typing import List, Tuple, Optional

# Constants
SCREEN_WIDTH = 800
SCREEN_HEIGHT = 600
HUD_HEIGHT = 100
FOV = 60
NUM_RAYS = SCREEN_WIDTH
MAX_DEPTH = 1000
WALL_HEIGHT = 1.0

@dataclass
class WADLump:
    name: str
    offset: int
    size: int

class WADLoader:
    """Load and parse DOOM WAD files"""
    def __init__(self, filepath):
        self.filepath = filepath
        self.lumps = []
        self.palette = []
        self.colormap = []
        self.load_wad()
    
    def load_wad(self):
        with open(self.filepath, 'rb') as f:
            header = f.read(4)
            num_lumps = struct.unpack('<I', f.read(4))[0]
            info_offset = struct.unpack('<I', f.read(4))[0]
            
            f.seek(info_offset)
            for _ in range(num_lumps):
                offset = struct.unpack('<I', f.read(4))[0]
                size = struct.unpack('<I', f.read(4))[0]
                name = f.read(8).decode('ascii', errors='ignore').rstrip('\x00')
                self.lumps.append(WADLump(name, offset, size))
            
            # Load palette
            for lump in self.lumps:
                if lump.name.upper() == 'PLAYPAL':
                    f.seek(lump.offset)
                    data = f.read(lump.size)
                    for i in range(min(256, len(data) // 3)):
                        r, g, b = data[i*3], data[i*3+1], data[i*3+2]
                        self.palette.append((r, g, b))
                    break
            
            # Load colormap
            for lump in self.lumps:
                if lump.name.upper() == 'COLORMAP':
                    f.seek(lump.offset)
                    self.colormap = list(f.read(lump.size))
                    break
    
    def get_lump(self, name):
        for lump in self.lumps:
            if lump.name.upper() == name.upper():
                return lump
        return None
    
    def read_lump_data(self, name):
        lump = self.get_lump(name)
        if lump:
            with open(self.filepath, 'rb') as f:
                f.seek(lump.offset)
                return f.read(lump.size)
        return None
    
    def get_map_header(self, map_name):
        """Get map header (E1M1, MAP01, etc.)"""
        return self.get_lump(map_name.upper())
    
    def get_level_data(self, map_name):
        """Get all data for a level"""
        prefixes = ['THINGS', 'LINEDEFS', 'SIDEDEFS', 'VERTEXES', 'SEGS', 'SSECTORS', 'NODES', 'SECTORS', 'REJECT', 'BLOCKMAP']
        data = {}
        for prefix in prefixes:
            lump_name = f"{map_name}{prefix}"
            lump = self.get_lump(lump_name)
            if lump:
                with open(self.filepath, 'rb') as f:
                    f.seek(lump.offset)
                    data[prefix] = f.read(lump.size)
            else:
                data[prefix] = None
        return data


class Player:
    def __init__(self, x, y, angle=0):
        self.x = x
        self.y = y
        self.angle = angle
        self.health = 100
        self.ammo = 50
        self.weapon = 1
        self.kills = 0
    
    def move(self, dx, dy, game_map):
        new_x = self.x + dx
        new_y = self.y + dy
        if not game_map.is_wall(int(new_x), int(self.y)):
            self.x = new_x
        if not game_map.is_wall(int(self.x), int(new_y)):
            self.y = new_y
    
    def rotate(self, da):
        self.angle = (self.angle + da) % (2 * math.pi)


class GameMap:
    def __init__(self, width=64, height=64):
        self.width = width
        self.height = height
        self.walls = [[0 for _ in range(width)] for _ in range(height)]
        self.floor_color = (40, 35, 30)
        self.ceiling_color = (20, 20, 25)
        self.spawn_x = 2
        self.spawn_y = 2
        self.spawn_angle = 0
        self.things = []
        self.exit_pos = None
    
    def load_from_json(self, data):
        self.walls = [[0 for _ in range(self.width)] for _ in range(self.height)]
        
        if 'walls' in data:
            for x, y, wall_type in data['walls']:
                if 0 <= x < self.width and 0 <= y < self.height:
                    self.walls[y][x] = wall_type
        
        if 'spawn' in data:
            self.spawn_x = data['spawn'].get('x', 2)
            self.spawn_y = data['spawn'].get('y', 2)
            self.spawn_angle = data['spawn'].get('angle', 0)
        
        if 'floor_color' in data:
            self.floor_color = tuple(data['floor_color'])
        if 'ceiling_color' in data:
            self.ceiling_color = tuple(data['ceiling_color'])
        
        if 'exit' in data:
            self.exit_pos = (data['exit']['x'], data['exit']['y'])
    
    def is_wall(self, x, y):
        if x < 0 or x >= self.width or y < 0 or y >= self.height:
            return True
        return self.walls[y][x] > 0


class Game:
    def __init__(self):
        pygame.init()
        self.screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
        pygame.display.set_caption("DOOM Dark Ages - Nokia1709")
        
        # Create icon
        self.create_icon()
        
        self.clock = pygame.time.Clock()
        self.font_large = pygame.font.Font(None, 72)
        self.font_medium = pygame.font.Font(None, 48)
        self.font_small = pygame.font.Font(None, 32)
        
        self.state = "TITLE"  # TITLE, WAD_SELECT, PLAYING, PAUSED
        self.current_wad = None
        self.maps = []
        self.current_level = 0
        
        # Title screen animation
        self.title_timer = 0
        self.flame_offset = 0
        
        # Load built-in levels
        self.load_builtin_levels()
        
        # Load sounds
        self.sounds = {}
        self.init_sounds()
    
    def create_icon(self):
        """Create game icon"""
        icon = pygame.Surface((32, 32))
        icon.fill((139, 0, 0))
        pygame.draw.rect(icon, (255, 69, 0), (2, 2, 28, 28), 2)
        
        # Eyes
        pygame.draw.circle(icon, (255, 215, 0), (10, 12), 4)
        pygame.draw.circle(icon, (255, 215, 0), (22, 12), 4)
        
        # Mouth
        pygame.draw.rect(icon, (0, 0, 0), (8, 20, 16, 6))
        
        pygame.display.set_icon(icon)
    
    def init_sounds(self):
        """Initialize sound effects"""
        try:
            pygame.mixer.init(frequency=22050, size=-16, channels=2, buffer=512)
        except:
            pass
        
        # Generate sounds using numpy
        self.generate_sounds()
    
    def generate_sounds(self):
        """Generate game sounds using numpy"""
        import numpy as np
        
        sample_rate = 22050
        
        # Menu beep
        t = np.linspace(0, 0.1, int(sample_rate * 0.1))
        beep = (np.sin(2 * np.pi * 440 * t) * 32767 * 0.3).astype(np.int16)
        stereo = np.column_stack((beep, beep))
        try:
            self.sounds['menu_select'] = pygame.sndarray.make_sound(stereo)
        except:
            pass
        
        # Weapon shot
        t = np.linspace(0, 0.15, int(sample_rate * 0.15))
        noise = np.random.uniform(-1, 1, len(t))
        shot = (noise * 32767 * 0.4).astype(np.int16)
        stereo = np.column_stack((shot, shot))
        try:
            self.sounds['shoot'] = pygame.sndarray.make_sound(stereo)
        except:
            pass
        
        # Pickup sound
        t = np.linspace(0, 0.2, int(sample_rate * 0.2))
        pickup = (np.sin(2 * np.pi * 880 * t) * 32767 * 0.2).astype(np.int16)
        stereo = np.column_stack((pickup, pickup))
        try:
            self.sounds['pickup'] = pygame.sndarray.make_sound(stereo)
        except:
            pass
    
    def play_sound(self, name):
        if name in self.sounds:
            try:
                self.sounds[name].play()
            except:
                pass
    
    def load_builtin_levels(self):
        """Load built-in levels"""
        levels_dir = 'levels'
        if os.path.exists(levels_dir):
            for i in range(1, 26):
                level_file = os.path.join(levels_dir, f'level_{i:02d}.json')
                if os.path.exists(level_file):
                    try:
                        with open(level_file, 'r') as f:
                            data = json.load(f)
                            self.maps.append(data)
                    except:
                        pass
        
        if not self.maps:
            # Create default map
            self.maps.append(self.create_default_level())
    
    def create_default_level(self):
        """Create a default castle level"""
        walls = []
        size = 32
        
        # Outer walls
        for x in range(size):
            walls.append((x, 0, 1))
            walls.append((x, size-1, 1))
        for y in range(size):
            walls.append((0, y, 1))
            walls.append((size-1, y, 1))
        
        # Some internal walls
        for x in range(5, 15):
            walls.append((x, 10, 1))
        for x in range(20, 28):
            walls.append((x, 8, 1))
        
        return {
            'name': 'The Dark Castle',
            'width': size,
            'height': size,
            'walls': walls,
            'spawn': {'x': 5, 'y': 5, 'angle': 0},
            'floor_color': [50, 40, 35],
            'ceiling_color': [25, 20, 30],
            'exit': {'x': 28, 'y': 28}
        }
    
    def draw_title_screen(self):
        """Draw Nokia1709 title screen"""
        self.screen.fill((0, 0, 0))
        
        # Animated flames at bottom
        self.flame_offset = (self.flame_offset + 1) % 100
        for x in range(0, SCREEN_WIDTH, 20):
            h = 30 + int(20 * math.sin((x + self.flame_offset) * 0.1))
            color = (255, max(0, 100 - h), 0)
            pygame.draw.rect(self.screen, color, (x, SCREEN_HEIGHT - h, 20, h))
        
        # Dark gradient background
        for y in range(SCREEN_HEIGHT - 100):
            shade = int(20 + (y / SCREEN_HEIGHT) * 20)
            pygame.draw.line(self.screen, (shade, shade//2, shade//3), 
                           (0, y), (SCREEN_WIDTH, y))
        
        # DOOM text with glow effect
        for offset in range(5, 0, -1):
            glow_color = (offset * 30, offset * 10, 0)
            text = self.font_large.render("DOOM", True, glow_color)
            text_rect = text.get_rect(center=(SCREEN_WIDTH // 2, 150 - offset))
            self.screen.blit(text, text_rect)
        
        # THE DARK AGES
        text = self.font_large.render("THE DARK AGES", True, (180, 50, 20))
        text_rect = text.get_rect(center=(SCREEN_WIDTH // 2, 220))
        self.screen.blit(text, text_rect)
        
        # Nokia1709 branding
        text = self.font_medium.render("By Nokia1709", True, (100, 100, 100))
        text_rect = text.get_rect(center=(SCREEN_WIDTH // 2, 280))
        self.screen.blit(text, text_rect)
        
        # Menu options
        options = [
            "[1] FREEDOOM 1 (DOOM)",
            "[2] FREEDOOM 2 (DOOM II)",
            "[3] BUILT-IN LEVELS",
            "[ESC] EXIT"
        ]
        
        y = 360
        for i, opt in enumerate(options):
            color = (150, 150, 150) if i % 2 == 0 else (120, 120, 120)
            text = self.font_small.render(opt, True, color)
            text_rect = text.get_rect(center=(SCREEN_WIDTH // 2, y))
            self.screen.blit(text, text_rect)
            y += 40
        
        # Version
        text = self.font_small.render("v2.0", True, (80, 80, 80))
        self.screen.blit(text, (10, SCREEN_HEIGHT - 30))
        
        # Controls hint
        text = self.font_small.render("WASD: Move | Mouse: Look | Click: Shoot | ESC: Menu", True, (60, 60, 60))
        text_rect = text.get_rect(center=(SCREEN_WIDTH // 2, SCREEN_HEIGHT - 50))
        self.screen.blit(text, text_rect)
    
    def draw_wad_select(self):
        """Draw WAD file selection screen"""
        self.screen.fill((10, 10, 15))
        
        text = self.font_large.render("SELECT WAD FILE", True, (200, 100, 50))
        text_rect = text.get_rect(center=(SCREEN_WIDTH // 2, 80))
        self.screen.blit(text, text_rect)
        
        # Find WAD files
        wad_files = []
        search_paths = ['.', 'assets', os.path.expanduser('~')]
        
        for path in search_paths:
            if os.path.exists(path):
                for f in os.listdir(path):
                    if f.lower().endswith('.wad'):
                        full_path = os.path.join(path, f)
                        size = os.path.getsize(full_path) // (1024 * 1024)
                        wad_files.append((f, full_path, size))
        
        y = 150
        if wad_files:
            for i, (name, path, size) in enumerate(wad_files):
                color = (150, 150, 150)
                text = self.font_small.render(f"[{i+1}] {name} ({size}MB)", True, color)
                self.screen.blit(text, (100, y))
                y += 35
        else:
            text = self.font_small.render("No WAD files found!", True, (200, 50, 50))
            text_rect = text.get_rect(center=(SCREEN_WIDTH // 2, y))
            self.screen.blit(text, text_rect)
            y += 50
            text = self.font_small.render("Put .WAD files in the game folder", True, (100, 100, 100))
            text_rect = text.get_rect(center=(SCREEN_WIDTH // 2, y))
            self.screen.blit(text, text_rect)
        
        y += 50
        text = self.font_small.render("[B] BUILT-IN LEVELS", True, (120, 120, 120))
        self.screen.blit(text, (100, y))
        y += 40
        text = self.font_small.render("[ESC] BACK", True, (120, 120, 120))
        self.screen.blit(text, (100, y))
        
        # Store wad files for selection
        self.available_wads = wad_files
    
    def draw_hud(self, player):
        """Draw game HUD"""
        hud_y = SCREEN_HEIGHT - HUD_HEIGHT
        
        # HUD background
        pygame.draw.rect(self.screen, (20, 20, 20), (0, hud_y, SCREEN_WIDTH, HUD_HEIGHT))
        pygame.draw.line(self.screen, (100, 50, 30), (0, hud_y), (SCREEN_WIDTH, hud_y), 2)
        
        # Health
        health_color = (50, 200, 50) if player.health > 50 else (200, 50, 50)
        text = self.font_small.render(f"HEALTH: {player.health}", True, health_color)
        self.screen.blit(text, (20, hud_y + 20))
        pygame.draw.rect(self.screen, (100, 30, 30), (20, hud_y + 50, 150, 20))
        pygame.draw.rect(self.screen, health_color, (20, hud_y + 50, int(1.5 * player.health), 20))
        
        # Ammo
        text = self.font_small.render(f"AMMO: {player.ammo}", True, (200, 200, 50))
        self.screen.blit(text, (200, hud_y + 20))
        pygame.draw.rect(self.screen, (50, 50, 30), (200, hud_y + 50, 150, 20))
        pygame.draw.rect(self.screen, (200, 200, 50), (200, hud_y + 50, min(150, player.ammo), 20))
        
        # Weapon
        weapons = {1: "FIST", 2: "PISTOL", 3: "SHOTGUN", 4: "CHAINGUN", 
                   5: "ROCKET", 6: "PLASMA", 7: "BFG", 8: "CHAINSAW"}
        text = self.font_small.render(f"WEAPON: {weapons.get(player.weapon, 'PISTOL')}", True, (150, 150, 200))
        self.screen.blit(text, (400, hud_y + 20))
        
        # Kills
        text = self.font_small.render(f"KILLS: {player.kills}", True, (200, 100, 100))
        self.screen.blit(text, (400, hud_y + 50))
        
        # Level name
        if self.current_level < len(self.maps):
            level_name = self.maps[self.current_level].get('name', f'LEVEL {self.current_level + 1}')
            text = self.font_medium.render(level_name, True, (180, 100, 60))
            text_rect = text.get_rect(center=(SCREEN_WIDTH // 2, hud_y + 35))
            self.screen.blit(text, text_rect)
        
        # Crosshair
        cx, cy = SCREEN_WIDTH // 2, (SCREEN_HEIGHT - HUD_HEIGHT) // 2
        pygame.draw.circle(self.screen, (200, 200, 200), (cx, cy), 3, 1)
        pygame.draw.line(self.screen, (200, 200, 200), (cx - 10, cy), (cx - 5, cy))
        pygame.draw.line(self.screen, (200, 200, 200), (cx + 5, cy), (cx + 10, cy))
        pygame.draw.line(self.screen, (200, 200, 200), (cx, cy - 10), (cx, cy - 5))
        pygame.draw.line(self.screen, (200, 200, 200), (cx, cy + 5), (cx, cy + 10))
    
    def draw_minimap(self, game_map, player):
        """Draw minimap"""
        map_size = 100
        map_x = SCREEN_WIDTH - map_size - 20
        map_y = SCREEN_HEIGHT - HUD_HEIGHT - map_size - 20
        
        # Background
        pygame.draw.rect(self.screen, (20, 20, 20), (map_x - 2, map_y - 2, map_size + 4, map_size + 4))
        pygame.draw.rect(self.screen, (40, 40, 40), (map_x, map_y, map_size, map_size))
        
        # Scale factor
        scale = map_size / max(game_map.width, game_map.height)
        
        # Draw walls
        for y in range(game_map.height):
            for x in range(game_map.width):
                if game_map.walls[y][x] > 0:
                    wx = int(map_x + x * scale)
                    wy = int(map_y + y * scale)
                    pygame.draw.rect(self.screen, (100, 80, 60), (wx, wy, max(1, int(scale)), max(1, int(scale))))
        
        # Draw player
        px = int(map_x + player.x * scale)
        py = int(map_y + player.y * scale)
        pygame.draw.circle(self.screen, (255, 200, 100), (px, py), 3)
        
        # Draw direction
        dx = math.cos(player.angle) * 10
        dy = math.sin(player.angle) * 10
        pygame.draw.line(self.screen, (255, 255, 100), (px, py), (int(px + dx), int(py + dy)), 1)
    
    def draw_level_complete(self):
        """Draw level complete screen"""
        overlay = pygame.Surface((SCREEN_WIDTH, SCREEN_HEIGHT))
        overlay.set_alpha(180)
        overlay.fill((0, 0, 0))
        self.screen.blit(overlay, (0, 0))
        
        text = self.font_large.render("LEVEL COMPLETE!", True, (100, 200, 100))
        text_rect = text.get_rect(center=(SCREEN_WIDTH // 2, SCREEN_HEIGHT // 2 - 50))
        self.screen.blit(text, text_rect)
        
        text = self.font_medium.render("Press ENTER for next level", True, (150, 150, 150))
        text_rect = text.get_rect(center=(SCREEN_WIDTH // 2, SCREEN_HEIGHT // 2 + 20))
        self.screen.blit(text, text_rect)
        
        text = self.font_small.render("Press ESC for menu", True, (100, 100, 100))
        text_rect = text.get_rect(center=(SCREEN_WIDTH // 2, SCREEN_HEIGHT // 2 + 60))
        self.screen.blit(text, text_rect)
    
    def draw_pause_menu(self):
        """Draw pause menu"""
        overlay = pygame.Surface((SCREEN_WIDTH, SCREEN_HEIGHT))
        overlay.set_alpha(200)
        overlay.fill((0, 0, 0))
        self.screen.blit(overlay, (0, 0))
        
        text = self.font_large.render("PAUSED", True, (200, 100, 50))
        text_rect = text.get_rect(center=(SCREEN_WIDTH // 2, 150))
        self.screen.blit(text, text_rect)
        
        options = [
            "[ENTER] RESUME",
            "[R] RESTART LEVEL",
            "[S] SAVE GAME",
            "[ESC] MAIN MENU"
        ]
        
        y = 250
        for opt in options:
            text = self.font_medium.render(opt, True, (150, 150, 150))
            text_rect = text.get_rect(center=(SCREEN_WIDTH // 2, y))
            self.screen.blit(text, text_rect)
            y += 50
    
    def render_raycasted_view(self, player, game_map):
        """Simple raycasting renderer"""
        height = SCREEN_HEIGHT - HUD_HEIGHT
        
        # Draw ceiling
        pygame.draw.rect(self.screen, game_map.ceiling_color, (0, 0, SCREEN_WIDTH, height // 2))
        
        # Draw floor
        pygame.draw.rect(self.screen, game_map.floor_color, (0, height // 2, SCREEN_WIDTH, height // 2))
        
        # Raycasting
        angle_step = FOV / NUM_RAYS
        ray_dir = player.angle - math.radians(FOV / 2)
        
        for ray in range(NUM_RAYS):
            ray_x = math.cos(ray_dir)
            ray_y = math.sin(ray_dir)
            
            distance = 0
            hit = False
            
            # Cast ray
            while distance < MAX_DEPTH and not hit:
                distance += 0.05
                test_x = player.x + ray_x * distance
                test_y = player.y + ray_y * distance
                
                if game_map.is_wall(int(test_x), int(test_y)):
                    hit = True
            
            # Fix fisheye
            corrected_dist = distance * math.cos(ray_dir - player.angle)
            
            # Calculate wall height
            wall_height = int((height / corrected_dist) * WALL_HEIGHT * 2) if corrected_dist > 0 else height
            
            # Shade based on distance
            shade = max(0, 255 - int(corrected_dist * 8))
            color = (shade // 3, shade // 3, shade // 2)
            
            # Draw wall column
            wall_top = (height - wall_height) // 2
            if wall_top < 0:
                wall_top = 0
            
            pygame.draw.line(self.screen, color, 
                           (ray, wall_top), 
                           (ray, min(wall_top + wall_height, height)))
            
            ray_dir += math.radians(angle_step)
    
    def handle_input(self):
        """Handle input events"""
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return False
            
            if event.type == pygame.KEYDOWN:
                if self.state == "TITLE":
                    if event.key == pygame.K_1:
                        # Try to load Freedoom 1
                        self.try_load_wad('freedoom1.wad') or \
                        self.try_load_wad('assets/freedoom1.wad') or \
                        self.try_load_wad('freedoom.wad')
                        if not self.current_wad:
                            self.state = "WAD_SELECT"
                    elif event.key == pygame.K_2:
                        self.try_load_wad('freedoom2.wad') or \
                        self.try_load_wad('assets/freedoom2.wad')
                        if not self.current_wad:
                            self.state = "WAD_SELECT"
                    elif event.key == pygame.K_3:
                        self.current_wad = None
                        self.current_level = 0
                        self.start_level()
                    elif event.key == pygame.K_ESCAPE:
                        return False
                
                elif self.state == "WAD_SELECT":
                    if event.key == pygame.K_1 and hasattr(self, 'available_wads') and len(self.available_wads) >= 1:
                        _, path, _ = self.available_wads[0]
                        self.current_wad = WADLoader(path)
                        self.current_level = 0
                        self.start_level()
                    elif event.key == pygame.K_2 and hasattr(self, 'available_wads') and len(self.available_wads) >= 2:
                        _, path, _ = self.available_wads[1]
                        self.current_wad = WADLoader(path)
                        self.current_level = 0
                        self.start_level()
                    elif event.key == pygame.K_b:
                        self.current_wad = None
                        self.current_level = 0
                        self.start_level()
                    elif event.key == pygame.K_ESCAPE:
                        self.state = "TITLE"
                
                elif self.state == "PLAYING":
                    if event.key == pygame.K_ESCAPE:
                        self.state = "PAUSED"
                    elif event.key == pygame.K_1:
                        self.player.weapon = 1
                    elif event.key == pygame.K_2:
                        self.player.weapon = 2
                    elif event.key == pygame.K_3:
                        self.player.weapon = 3
                
                elif self.state == "PAUSED":
                    if event.key == pygame.K_ESCAPE:
                        self.state = "TITLE"
                    elif event.key == pygame.K_RETURN:
                        self.state = "PLAYING"
                    elif event.key == pygame.K_r:
                        self.start_level()
                    elif event.key == pygame.K_s:
                        self.save_game()
                
                elif self.state == "LEVEL_COMPLETE":
                    if event.key == pygame.K_RETURN:
                        self.current_level += 1
                        if self.current_level >= len(self.maps):
                            self.current_level = 0
                        self.start_level()
                    elif event.key == pygame.K_ESCAPE:
                        self.state = "TITLE"
            
            elif event.type == pygame.MOUSEBUTTONDOWN:
                if self.state == "PLAYING" and event.button == 1:
                    self.player.ammo -= 1
                    self.play_sound('shoot')
        
        return True
    
    def try_load_wad(self, path):
        """Try to load a WAD file"""
        if os.path.exists(path):
            try:
                self.current_wad = WADLoader(path)
                self.current_level = 0
                self.start_level()
                return True
            except Exception as e:
                print(f"Error loading {path}: {e}")
        return False
    
    def start_level(self):
        """Start the current level"""
        if self.current_wad:
            # Load from WAD - find first map
            map_names = []
            for i in range(1, 10):
                map_names.append(f'E1M{i}')
                map_names.append(f'MAP{i:02d}')
            
            for name in map_names:
                header = self.current_wad.get_map_header(name)
                if header:
                    self.current_level_name = name
                    break
            else:
                self.current_level_name = 'E1M1'
            
            # Create simple map from WAD
            self.game_map = GameMap(32, 32)
            self.setup_wad_level()
        else:
            # Load from built-in levels
            if self.current_level < len(self.maps):
                level_data = self.maps[self.current_level]
                self.game_map = GameMap(level_data['width'], level_data['height'])
                self.game_map.load_from_json(level_data)
            else:
                self.game_map = GameMap()
        
        # Create player
        self.player = Player(
            self.game_map.spawn_x, 
            self.game_map.spawn_y, 
            self.game_map.spawn_angle
        )
        
        self.state = "PLAYING"
    
    def setup_wad_level(self):
        """Setup level from WAD data"""
        # Create a default castle-style map for now
        size = 32
        self.game_map.walls = [[0 for _ in range(size)] for _ in range(size)]
        
        # Outer walls
        for x in range(size):
            self.game_map.walls[0][x] = 1
            self.game_map.walls[size-1][x] = 1
        for y in range(size):
            self.game_map.walls[y][0] = 1
            self.game_map.walls[y][size-1] = 1
        
        # Some interior structures
        for x in range(8, 16):
            self.game_map.walls[8][x] = 1
        for x in range(18, 26):
            self.game_map.walls[12][x] = 1
        for y in range(16, 24):
            self.game_map.walls[y][16] = 1
        
        # Entry point
        self.game_map.spawn_x = 4
        self.game_map.spawn_y = 4
        self.game_map.spawn_angle = 0
        self.game_map.exit_pos = (28, 28)
        
        self.game_map.floor_color = (50, 40, 35)
        self.game_map.ceiling_color = (25, 20, 30)
    
    def save_game(self):
        """Save game state"""
        save_data = {
            'level': self.current_level,
            'player_x': self.player.x,
            'player_y': self.player.y,
            'player_angle': self.player.angle,
            'health': self.player.health,
            'ammo': self.player.ammo,
            'kills': self.player.kills,
            'weapon': self.player.weapon
        }
        
        try:
            with open('savegame.json', 'w') as f:
                json.dump(save_data, f)
            self.play_sound('pickup')
        except:
            pass
    
    def update(self):
        """Update game state"""
        if self.state != "PLAYING":
            return
        
        keys = pygame.key.get_pressed()
        mouse_rel = pygame.mouse.get_rel()
        
        # Movement
        speed = 0.08
        rot_speed = 0.03
        
        dx = 0
        dy = 0
        
        if keys[pygame.K_w]:
            dx += math.cos(self.player.angle) * speed
            dy += math.sin(self.player.angle) * speed
        if keys[pygame.K_s]:
            dx -= math.cos(self.player.angle) * speed
            dy -= math.sin(self.player.angle) * speed
        if keys[pygame.K_a]:
            dx += math.cos(self.player.angle - math.pi/2) * speed
            dy += math.sin(self.player.angle - math.pi/2) * speed
        if keys[pygame.K_d]:
            dx -= math.cos(self.player.angle - math.pi/2) * speed
            dy -= math.sin(self.player.angle - math.pi/2) * speed
        
        if keys[pygame.K_LEFT]:
            self.player.rotate(-rot_speed)
        if keys[pygame.K_RIGHT]:
            self.player.rotate(rot_speed)
        if keys[pygame.K_UP]:
            dx += math.cos(self.player.angle) * speed
            dy += math.sin(self.player.angle) * speed
        if keys[pygame.K_DOWN]:
            dx -= math.cos(self.player.angle) * speed
            dy -= math.sin(self.player.angle) * speed
        
        # Mouse look
        if pygame.mouse.get_focused():
            self.player.rotate(mouse_rel[0] * 0.002)
        
        self.player.move(dx, dy, self.game_map)
        
        # Check exit
        if self.game_map.exit_pos:
            ex, ey = self.game_map.exit_pos
            if abs(self.player.x - ex) < 1 and abs(self.player.y - ey) < 1:
                self.state = "LEVEL_COMPLETE"
                self.play_sound('pickup')
    
    def draw(self):
        """Draw game"""
        if self.state == "TITLE":
            self.draw_title_screen()
        elif self.state == "WAD_SELECT":
            self.draw_wad_select()
        elif self.state == "PLAYING" or self.state == "LEVEL_COMPLETE":
            # Draw 3D view
            self.render_raycasted_view(self.player, self.game_map)
            
            # HUD
            self.draw_hud(self.player)
            
            # Minimap
            self.draw_minimap(self.game_map, self.player)
            
            if self.state == "LEVEL_COMPLETE":
                self.draw_level_complete()
        
        elif self.state == "PAUSED":
            self.render_raycasted_view(self.player, self.game_map)
            self.draw_hud(self.player)
            self.draw_minimap(self.game_map, self.player)
            self.draw_pause_menu()
        
        pygame.display.flip()
    
    def run(self):
        """Main game loop"""
        running = True
        
        while running:
            running = self.handle_input()
            self.update()
            self.draw()
            self.clock.tick(60)
        
        pygame.quit()
        sys.exit()


if __name__ == "__main__":
    game = Game()
    game.run()

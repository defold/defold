#!/usr/bin/env python
import os
import json
import time
from datetime import datetime

class BuildTimeTracker:
    """
    Tracks build times for commands and engine components.
    Provides functionality to log, save, and analyze build performance.
    """
    
    def __init__(self, output_file=None, logger=None):
        self.start_times = {}
        self.end_times = {}
        self.component_times = {}
        self.command_times = {}
        self.output_file = output_file or "build_times.json"
        self.current_command = None
        self.logger = logger or print
        
    def start_command(self, command_name):
        """Start timing a build command"""
        self.current_command = command_name
        self.start_times[command_name] = time.time()
        self.logger(f"Starting '{command_name}' at {datetime.now().strftime('%H:%M:%S')}")
        
    def end_command(self, command_name):
        """End timing a build command"""
        if command_name in self.start_times:
            end_time = time.time()
            duration = end_time - self.start_times[command_name]
            self.command_times[command_name] = {
                'start_time': self.start_times[command_name],
                'end_time': end_time,
                'duration': duration,
                'timestamp': datetime.now().isoformat()
            }
            self.logger(f"'{command_name}' completed in {duration:.2f} s")

    @staticmethod
    def read_ninja_log(build_dir):
        """Snapshot log records so unchanged builds and log compaction do not replay old timings."""
        try:
            with open(os.path.join(build_dir, '.ninja_log'), encoding='utf-8') as f:
                return set(f.read().splitlines())
        except FileNotFoundError:
            return set()

    def record_ninja_library_times(self, command_name, build_dir, previous_log):
        """Attach per-library timings from tasks executed by this command."""
        library_tasks = {}
        for line in self.read_ninja_log(build_dir) - previous_log:
            parts = line.split('\t')
            if len(parts) != 5:
                continue
            try:
                start, end = int(parts[0]), int(parts[1])
            except ValueError:
                continue
            if end < start:
                continue
            output = parts[3]
            if os.path.isabs(output):
                output = os.path.relpath(output, build_dir)
            output = output.replace('\\', '/')
            library, separator, _ = output.partition('/')
            if not separator or library in ('CMakeFiles', '..'):
                continue
            # Ninja logs each output of a command separately, including absolute aliases.
            library_tasks.setdefault(library, set()).add((start, end, parts[4]))

        self.command_times[command_name]['libraries'] = {
            library: {
                'elapsed_span': (max(end for _, end, _ in tasks) - min(start for start, _, _ in tasks)) / 1000,
            }
            for library, tasks in library_tasks.items()
        }
            
    def start_component(self, component_name, platform=None):
        """Start timing an engine component build"""
        key = f"{component_name}_{platform}" if platform else component_name
        self.start_times[key] = time.time()
        
        # Log the start of component build
        platform_str = f" for {platform}" if platform else ""
        self.logger(f"Building {component_name}{platform_str}")
        
    def end_component(self, component_name, platform=None):
        """End timing an engine component build"""
        key = f"{component_name}_{platform}" if platform else component_name
        if key in self.start_times:
            end_time = time.time()
            duration = end_time - self.start_times[key]
            
            if component_name not in self.component_times:
                self.component_times[component_name] = {}
            
            self.component_times[component_name][platform or 'default'] = {
                'start_time': self.start_times[key],
                'end_time': end_time,
                'duration': duration,
                'timestamp': datetime.now().isoformat()
            }
            
            self.logger(f"  {component_name} ({platform}) completed in {duration:.2f} s")
            
    def get_summary(self):
        """Get a summary of all build times"""
        summary = {
            'build_session': {
                'start_time': min(self.start_times.values()) if self.start_times else None,
                'end_time': max(self.end_times.values()) if self.end_times else None,
                'total_duration': sum(cmd['duration'] for cmd in self.command_times.values()) if self.command_times else 0,
                'timestamp': datetime.now().isoformat()
            },
            'commands': self.command_times,
            'components': self.component_times
        }
        
        # Calculate component totals
        component_totals = {}
        for component, platforms in self.component_times.items():
            total_duration = sum(platform_data['duration'] for platform_data in platforms.values())
            component_totals[component] = {
                'total_duration': total_duration,
                'platforms': len(platforms),
                'average_per_platform': total_duration / len(platforms) if platforms else 0
            }
        
        summary['component_totals'] = component_totals
        return summary
        
    def save_times(self, filename=None):
        """Save build times to JSON file"""
        output_file = filename or self.output_file
        summary = self.get_summary()
        
        # Save only the current build session (overwrite file)
        with open(output_file, 'w') as f:
            json.dump(summary, f, indent=2)
            
        self.logger(f"Build times saved to {output_file}")
        
    def print_summary(self):
        """Print a formatted summary of build times"""
        summary = self.get_summary()
        
        self.logger("\n" + "="*60)
        self.logger("BUILD TIME SUMMARY")
        self.logger("="*60)
        
        # Command times
        if self.command_times:
            self.logger("COMMAND TIMES:")
            command_width = max(20, max(len(command) for command in self.command_times))
            for cmd, data in sorted(self.command_times.items(), key=lambda x: x[1]['duration'], reverse=True):
                self.logger(f"  {cmd:<{command_width}} {data['duration']:>8.2f} s")
                if 'libraries' in data:
                    libraries = data['libraries']
                    if libraries:
                        self.logger("    Libraries")
                        for library, timing in sorted(libraries.items(), key=lambda item: item[1]['elapsed_span'], reverse=True):
                            self.logger(f"      {library:<20} {timing['elapsed_span']:>8.2f} s")
                    else:
                        self.logger("    No library tasks ran.")
        
        # Component times in tabular format
        if self.component_times:
            # Get all unique platforms
            all_platforms = set()
            for platforms in self.component_times.values():
                all_platforms.update(platforms.keys())
            
            if all_platforms:
                # Sort platforms alphabetically
                sorted_platforms = sorted(all_platforms)
                
                # Calculate column widths
                component_width = 15
                platform_width = 12  # "8.2f s" = 8 chars + 2 chars = 10, plus 2 for spacing
                
                # Create header with all platforms
                header = f"{'COMPONENTS:':<{component_width}} "
                for platform in sorted_platforms:
                    header += f"{platform:<{platform_width}} "
                header += "TOTAL    AVERAGE"
                self.logger(f"\n{header}")
                
                # Get all components that have data for any platform
                all_components = set()
                for component, platforms in self.component_times.items():
                    all_components.add(component)
                
                # Sort components by total time (descending)
                sorted_components = []
                for component in all_components:
                    platforms = self.component_times[component]
                    total_duration = sum(platform_data['duration'] for platform_data in platforms.values())
                    sorted_components.append((component, total_duration))
                
                sorted_components.sort(key=lambda x: x[1], reverse=True)
                
                for component, total_duration in sorted_components:
                    platforms = self.component_times[component]
                    avg_duration = total_duration / len(platforms)
                    
                    # Build the line with all platform times
                    line = f"{component:<{component_width}} "
                    for platform in sorted_platforms:
                        if platform in platforms:
                            line += f"{platforms[platform]['duration']:>8.2f} s "
                        else:
                            line += f"{'':>10} "  # 10 spaces to match "8.2f s "
                    line += f"{total_duration:>8.2f} s {avg_duration:>8.2f} s"
                    
                    self.logger(line)
        
        self.logger("="*60)

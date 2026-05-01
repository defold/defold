#!/usr/bin/env python
# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

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
            for cmd, data in sorted(self.command_times.items(), key=lambda x: x[1]['duration'], reverse=True):
                self.logger(f"  {cmd:<20} {data['duration']:>8.2f} s")
        
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
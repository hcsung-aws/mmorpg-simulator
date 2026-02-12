#!/usr/bin/env python3
"""
Protocol.h to JSON converter for PacketCaptureAgent
Parses C++ header file and generates protocol JSON automatically.
"""

import re
import json
import sys
from pathlib import Path

# C++ type to JSON type mapping
TYPE_MAP = {
    'uint8_t': 'uint8',
    'int8_t': 'int8',
    'uint16_t': 'uint16',
    'int16_t': 'int16',
    'uint32_t': 'uint32',
    'int32_t': 'int32',
    'uint64_t': 'uint64',
    'int64_t': 'int64',
    'char': 'string',
}

def parse_enum(content):
    """Parse PacketType enum to get packet names and values."""
    packets = {}
    enum_match = re.search(r'enum\s+PacketType[^{]*\{([^}]+)\}', content, re.DOTALL)
    if not enum_match:
        return packets
    
    enum_body = enum_match.group(1)
    for line in enum_body.split('\n'):
        match = re.match(r'\s*(\w+)\s*=\s*(0x[0-9A-Fa-f]+|[0-9]+)', line)
        if match:
            name = match.group(1)
            value = match.group(2)
            packets[name] = int(value, 16) if value.startswith('0x') else int(value)
    
    return packets

def parse_structs(content):
    """Parse struct definitions to get field information."""
    structs = {}
    # Match struct definitions
    struct_pattern = r'struct\s+(\w+)\s*\{([^}]+)\}'
    
    for match in re.finditer(struct_pattern, content, re.DOTALL):
        struct_name = match.group(1)
        struct_body = match.group(2)
        
        if struct_name == 'PacketHeader':
            continue
            
        fields = []
        for line in struct_body.split('\n'):
            line = line.strip()
            if not line or line.startswith('//'):
                continue
            
            # Match: type name; or type name[size];
            field_match = re.match(r'(\w+)\s+(\w+)(?:\[(\d+)\])?\s*;', line)
            if field_match:
                cpp_type = field_match.group(1)
                field_name = field_match.group(2)
                array_size = field_match.group(3)
                
                json_type = TYPE_MAP.get(cpp_type, cpp_type)
                
                field = {'name': field_name, 'type': json_type}
                if array_size:
                    field['length'] = int(array_size)
                
                fields.append(field)
        
        structs[struct_name] = fields
    
    return structs

def match_packet_to_struct(packet_name):
    """Convert enum name to struct name (CS_LOGIN -> CS_Login)."""
    parts = packet_name.split('_')
    if len(parts) >= 2:
        prefix = parts[0]  # CS or SC
        rest = '_'.join(parts[1:])
        # Convert to title case: LOGIN -> Login, CHAR_INFO -> CharInfo
        rest_parts = rest.split('_')
        rest_title = ''.join(p.capitalize() for p in rest_parts)
        return f"{prefix}_{rest_title}"
    return packet_name

def generate_protocol_json(header_path, output_path):
    """Generate protocol JSON from Protocol.h."""
    content = Path(header_path).read_text(encoding='utf-8')
    
    packets_enum = parse_enum(content)
    structs = parse_structs(content)
    
    protocol = {
        "protocol": {
            "name": "MMORPG Simulator",
            "version": "1.0",
            "endian": "little",
            "header": {
                "size_field": "length",
                "type_field": "type",
                "fields": [
                    {"name": "length", "type": "uint16", "offset": 0},
                    {"name": "type", "type": "uint16", "offset": 2}
                ]
            }
        },
        "packets": []
    }
    
    for packet_name, packet_type in sorted(packets_enum.items(), key=lambda x: x[1]):
        struct_name = match_packet_to_struct(packet_name)
        fields = structs.get(struct_name, [])
        
        direction = "C2S" if packet_name.startswith("CS_") else "S2C"
        
        packet_def = {
            "type": packet_type,
            "name": packet_name,
            "direction": direction,
            "fields": fields
        }
        protocol["packets"].append(packet_def)
    
    output = Path(output_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(protocol, indent=2), encoding='utf-8')
    
    print(f"Generated {output_path}")
    print(f"  - {len(packets_enum)} packets")
    print(f"  - {len(structs)} structs matched")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <Protocol.h> <output.json>")
        sys.exit(1)
    
    header_path = sys.argv[1]
    output_path = sys.argv[2]
    
    generate_protocol_json(header_path, output_path)

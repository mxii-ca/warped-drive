#pragma once


void parseNTFS(std::unique_ptr<BlockDevice> &device, uint8_t header[512]);

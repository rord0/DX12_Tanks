#include "ui.hpp"
#include "util.hpp"

const ShapeColor btn_tan_normal  = {.fillColor = ColorHexToRBGANormalized(0xB5944FFF), .strokeColor  = ColorHexToRBGANormalized(0x8E7545FF)};
const ShapeColor btn_tan_hovered = {.fillColor = ColorHexToRBGANormalized(0xB5944FFF), .strokeColor  = ColorHexToRBGANormalized(0x78633AFF)};
const ShapeColor btn_tan_pressed = {.fillColor = ColorHexToRBGANormalized(0xB5944FFF), .strokeColor  = ColorHexToRBGANormalized(0x262D33FF)};
const ButtonStyle BTN_TAN_STYLE = {btn_tan_normal, btn_tan_hovered, btn_tan_pressed, 6, 3};

const ShapeColor btn_dark_grey_normal  = {.fillColor = ColorHexToRBGANormalized(0x404E53FF), .strokeColor  = ColorHexToRBGANormalized(0x313F43FF)};
const ShapeColor btn_dark_grey_hovered = {.fillColor = ColorHexToRBGANormalized(0x404E53FF), .strokeColor  = ColorHexToRBGANormalized(0x262D33FF)};
const ShapeColor btn_dark_grey_pressed = {.fillColor = ColorHexToRBGANormalized(0x404E53FF), .strokeColor  = ColorHexToRBGANormalized(0x262D33FF)};
const ButtonStyle BTN_DARK_GREY_STYLE = {btn_dark_grey_normal, btn_dark_grey_hovered, btn_dark_grey_hovered, 6, 3};

const ShapeColor btn_green_normal = {.fillColor = ColorHexToRBGANormalized(0x6A9A44FF),  .strokeColor  = ColorHexToRBGANormalized(0x51763AFF)};
const ShapeColor btn_green_hovered = {.fillColor = ColorHexToRBGANormalized(0x6A9A44FF), .strokeColor  = ColorHexToRBGANormalized(0x4A623AFF)};
const ShapeColor btn_green_pressed = {.fillColor = ColorHexToRBGANormalized(0x6A9A44FF), .strokeColor  = ColorHexToRBGANormalized(0x262D33FF)};
const ButtonStyle BTN_GREEN_STYLE = {btn_green_normal, btn_green_hovered, btn_green_pressed, 6, 3};

const ShapeColor btn_grey_normal  = {.fillColor = ColorHexToRBGANormalized(0x7C878CFF), .strokeColor  = ColorHexToRBGANormalized(0x6B7376FF)};
const ShapeColor btn_grey_hovered = {.fillColor = ColorHexToRBGANormalized(0x7C878CFF), .strokeColor  = ColorHexToRBGANormalized(0x53595CFF)};
const ShapeColor btn_grey_pressed = {.fillColor = ColorHexToRBGANormalized(0x7C878CFF), .strokeColor  = ColorHexToRBGANormalized(0x262D33FF)};
const ButtonStyle BTN_GREY_STYLE = {btn_grey_normal, btn_grey_hovered, btn_grey_pressed, 6, 3};

SDFShapeStyle POPUP_CONTAINER_STYLE = {ColorHexToRBGANormalized(0x402525CC), ColorHexToRBGANormalized(0x332626FF), 6, 3};

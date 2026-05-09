"""Unified SirioNNUE2 layout contract shared by train_v2/export_to_engine_v2."""
from __future__ import annotations

MODEL_LAYOUT_NAME = "SirioNNUE2-MinimalV1"
MODEL_LAYOUT_VERSION = 1
FEATURE_SET = "SirioHalfKAv1"
FEATURES_PER_PERSPECTIVE = 40960
ACCUMULATOR_SIZE = 256
HIDDEN1_SIZE = 256
HIDDEN2_SIZE = 0
OUTPUT_SIZE = 1
ACTIVATION = "relu"
TENSOR_ORDER = (
    "input_embedding.weight",
    "hidden.bias",
    "output.weight",
    "output.bias",
)
BINARY_SECTION_ORDER = (
    "input_weights",
    "hidden_bias",
    "output_weights",
    "output_bias",
)
ENDIANNESS = "little"
QUANT_INPUT_SCALE = 256
QUANT_OUTPUT_SCALE = 256
EXPECTED_SCRIPT_NAME = "training.nnue.scripts.train_v2"

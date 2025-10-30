#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include "sirio/board.hpp"
#include "sirio/move.hpp"

namespace sirio {

namespace nnue {
enum class NetworkSelectionPolicy;
struct MultiNetworkConfig;
}

class EvaluationBackend {
public:
    virtual ~EvaluationBackend() = default;

    virtual void initialize(const Board &board) = 0;
    virtual void reset(const Board &board) = 0;
    virtual void push(const Board &current, const std::optional<Move> &move,
                      Color mover) = 0;
    virtual void pop() = 0;
    virtual int evaluate(const Board &board) = 0;

    [[nodiscard]] virtual std::unique_ptr<EvaluationBackend> clone() const = 0;
};

std::unique_ptr<EvaluationBackend> make_classical_evaluation();
std::unique_ptr<EvaluationBackend> make_nnue_evaluation(const std::string &path,
                                                        std::string *error_message = nullptr);
std::unique_ptr<EvaluationBackend> make_nnue_evaluation(
    const nnue::MultiNetworkConfig &config, std::string *error_message = nullptr);

std::size_t classical_evaluation_pawn_cache_misses();

void set_evaluation_backend(std::unique_ptr<EvaluationBackend> backend);
void use_classical_evaluation();
EvaluationBackend &active_evaluation_backend();

void initialize_evaluation(const Board &board);
void push_evaluation_state(Color mover, const std::optional<Move> &move,
                           const Board &current);
void pop_evaluation_state();

void notify_position_initialization(const Board &board);
void notify_move_applied(Color mover, const std::optional<Move> &move,
                         const Board &current);

int evaluate(const Board &board);

}  // namespace sirio


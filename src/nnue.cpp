#include "nnue.h"
#include "bitboard.h"
#include "incbin.h"
#include "position.h"
#include "uci.h"
#include "util.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

INCBIN(EmbeddedNNUE, EvalFile);

#define AsVecI(x) *(VecI*)(&x)
#define AsVecF(x) *(VecF*)(&x)

namespace {

std::filesystem::path executableDirectory() {
#if defined(_WIN32)
  wchar_t buf[MAX_PATH];
  DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (len == 0)
    return std::filesystem::current_path();
  std::wstring ws(buf, len);
  auto pos = ws.find_last_of(L"\\/");
  std::wstring dir = (pos == std::wstring::npos) ? std::wstring() : ws.substr(0, pos);
  if (dir.empty())
    return std::filesystem::current_path();
  int utf8Len = WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (utf8Len <= 1)
    return std::filesystem::path(dir);
  std::string utf8(utf8Len - 1, '\0');
  WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), -1, utf8.data(), utf8Len, nullptr, nullptr);
  return std::filesystem::path(utf8);
#else
  char buf[PATH_MAX];
#if defined(__APPLE__)
  uint32_t size = sizeof(buf);
  if (_NSGetExecutablePath(buf, &size) == 0)
    return std::filesystem::path(buf).parent_path();
#endif
#if defined(__linux__)
  ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (len > 0) {
    buf[len] = '\0';
    return std::filesystem::path(buf).parent_path();
  }
#endif
  return std::filesystem::current_path();
#endif
}

bool hasNetworkExtension(const std::filesystem::path& path) {
  auto ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return ext == ".nnue" || ext == ".bin";
}

std::optional<std::filesystem::path> findNetworkInDirectory(const std::filesystem::path& dir) {
  if (dir.empty())
    return std::nullopt;
  std::error_code ec;
  if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec))
    return std::nullopt;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec)
      break;
    if (!entry.is_regular_file(ec))
      continue;
    if (hasNetworkExtension(entry.path()))
      return entry.path();
  }
  return std::nullopt;
}

bool readBinaryFile(const std::filesystem::path& path, std::vector<uint8_t>& buffer) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return false;
  in.seekg(0, std::ios::end);
  std::streampos size = in.tellg();
  if (size <= 0)
    return false;
  buffer.resize(static_cast<size_t>(size));
  in.seekg(0, std::ios::beg);
  in.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
  return in.good();
}

} // namespace

namespace NNUE {

  constexpr int FloatInVec = sizeof(VecI) / sizeof(float);
  constexpr int I16InVec = sizeof(VecI) / sizeof(int16_t);
  constexpr int I8InVec = sizeof(VecI) / sizeof(int8_t);

  constexpr int FtShift = 9;

  struct Net {
    alignas(64) int16_t FeatureWeights[KingBuckets][2][6][64][L1];
    alignas(64) int16_t FeatureBiases[L1];

    union {
      alignas(64) int8_t L1Weights[OutputBuckets][L1][L2];
      alignas(64) int8_t L1WeightsAlt[OutputBuckets][L1 * L2];
    }; 
    alignas(64) float L1Biases[OutputBuckets][L2];

    alignas(64) float L2Weights[OutputBuckets][L2 * 2][L3];
    alignas(64) float L2Biases[OutputBuckets][L3];

    alignas(64) float L3Weights[OutputBuckets][L3];
    alignas(64) float L3Biases[OutputBuckets];
  };

  Net* Weights;

  // For every possible uint16 number, store the count of active bits,
  // and the index of each active bit
  alignas(Alignment) uint16_t nnzTable[256][8];

  namespace {
    bool useMaterialFallback = false;

    bool initializeNetworkFromBuffer(const uint8_t* data, size_t size) {
      if (!data || size < sizeof(Net))
        return false;

      if (!Weights)
        Weights = static_cast<Net*>(Util::allocAlign(sizeof(Net)));

      auto rawContent = std::make_unique<Net>();
      std::memcpy(rawContent.get(), data, sizeof(Net));
      std::memcpy(Weights, rawContent.get(), sizeof(Net));

      for (int bucket = 0; bucket < OutputBuckets; bucket++)
        for (int i = 0; i < L1; i += 4)
          for (int j = 0; j < L2; ++j)
            for (int k = 0; k < 4; k++)
              Weights->L1WeightsAlt[bucket][i * L2 + j * 4 + k] = rawContent->L1Weights[bucket][i + k][j];

      // Init NNZ table
      std::memset(nnzTable, 0, sizeof(nnzTable));
      for (int mask = 0; mask < 256; ++mask) {
        int j = 0;
        Bitboard bits = mask;
        while (bits)
          nnzTable[mask][j++] = popLsb(bits);
      }

      constexpr int weightsPerBlock = sizeof(__m128i) / sizeof(int16_t);
      constexpr int NumRegs = sizeof(VecI) / 8;
      __m128i regs[NumRegs];

      __m128i* ftWeights = (__m128i*) Weights->FeatureWeights;
      __m128i* ftBiases = (__m128i*) Weights->FeatureBiases;

      for (int i = 0; i < KingBuckets * 768 * L1 / weightsPerBlock; i += NumRegs) {
        for (int j = 0; j < NumRegs; j++)
          regs[j] = ftWeights[i + j];

        for (int j = 0; j < NumRegs; j++)
          ftWeights[i + j] = regs[PackusOrder[j]];
      }

      for (int i = 0; i < L1 / weightsPerBlock; i += NumRegs) {
        for (int j = 0; j < NumRegs; j++)
          regs[j] = ftBiases[i + j];

        for (int j = 0; j < NumRegs; j++)
          ftBiases[i + j] = regs[PackusOrder[j]];
      }

      useMaterialFallback = false;
      return true;
    }

    Score materialOnlyEval(const Position& pos) {
      static constexpr int pieceValues[PIECE_TYPE_NB] = {0, 100, 320, 330, 500, 900, 0, 0};
      int white = 0;
      int black = 0;
      for (PieceType pt = PAWN; pt <= QUEEN; pt = PieceType(pt + 1)) {
        white += pieceValues[pt] * BitCount(pos.pieces(WHITE, pt));
        black += pieceValues[pt] * BitCount(pos.pieces(BLACK, pt));
      }
      int diff = white - black;
      return pos.sideToMove == WHITE ? diff : -diff;
    }
  }


  bool needRefresh(Color side, Square oldKing, Square newKing) {
    // Crossed half?
    if ((oldKing & 0b100) != (newKing & 0b100))
      return true;

    return   KingBucketsScheme[relative_square(side, oldKing)]
          != KingBucketsScheme[relative_square(side, newKing)];
  }

  inline VecI* featureAddress(Square kingSq, Color side, Piece pc, Square sq) {
    if (kingSq & 0b100)
      sq = Square(sq ^ 7);

    return (VecI*) Weights->FeatureWeights
            [KingBucketsScheme[relative_square(side, kingSq)]]
            [side != piece_color(pc)]
            [piece_type(pc)-1]
            [relative_square(side, sq)];
  }

  template <int InputSize>
  inline void multiAdd(VecI* output, VecI* input, VecI* add0){
    for (int i = 0; i < InputSize / I16InVec; ++i)
      output[i] = addEpi16(input[i], add0[i]);
  }

  template <int InputSize>
  inline void multiSub(VecI* output, VecI* input, VecI* sub0){
    for (int i = 0; i < InputSize / I16InVec; ++i)
      output[i] = subEpi16(input[i], sub0[i]);
  }

  template <int InputSize>
  inline void multiAddAdd(VecI* output, VecI* input, VecI* add0, VecI* add1){
    for (int i = 0; i < InputSize / I16InVec; ++i)
      output[i] = addEpi16(input[i], addEpi16(add0[i], add1[i]));
  }

  template <int InputSize>
  inline void multiSubAdd(VecI* output, VecI* input, VecI* sub0, VecI* add0) {
    for (int i = 0; i < InputSize / I16InVec; ++i)
      output[i] = subEpi16(addEpi16(input[i], add0[i]), sub0[i]);
  }

  template <int InputSize>
  inline void multiSubAddSub(VecI* output, VecI* input, VecI* sub0, VecI* add0, VecI* sub1) {
    for (int i = 0; i < InputSize / I16InVec; ++i)
      output[i] = subEpi16(addEpi16(input[i], add0[i]), addEpi16(sub0[i], sub1[i]));
  }

  template <int InputSize>
  inline void multiSubAddSubAdd(VecI* output, VecI* input, VecI* sub0, VecI* add0, VecI* sub1, VecI* add1) {
    for (int i = 0; i < InputSize / I16InVec; ++i)
      output[i] = addEpi16(input[i], subEpi16(addEpi16(add0[i], add1[i]), addEpi16(sub0[i], sub1[i])));
  }

  void Accumulator::addPiece(Square kingSq, Color side, Piece pc, Square sq) {
    if (useMaterialFallback)
      return;
    multiAdd<L1>((VecI*) colors[side], (VecI*) colors[side], featureAddress(kingSq, side, pc, sq));
  }

  void Accumulator::movePiece(Square kingSq, Color side, Piece pc, Square from, Square to) {
    if (useMaterialFallback)
      return;
    multiSubAdd<L1>((VecI*) colors[side], (VecI*) colors[side],
     featureAddress(kingSq, side, pc, from), featureAddress(kingSq, side, pc, to));
  }

  void Accumulator::removePiece(Square kingSq, Color side, Piece pc, Square sq) {
    if (useMaterialFallback)
      return;
    multiSub<L1>((VecI*) colors[side], (VecI*) colors[side], featureAddress(kingSq, side, pc, sq));
  }

  void Accumulator::doUpdates(Square kingSq, Color side, Accumulator& input) {
    if (useMaterialFallback) {
      updated[side] = true;
      return;
    }
    DirtyPieces dp = this->dirtyPieces;
    if (dp.type == DirtyPieces::CASTLING) 
    {
      multiSubAddSubAdd<L1>((VecI*) colors[side], (VecI*) input.colors[side], 
        featureAddress(kingSq, side, dp.sub0.pc, dp.sub0.sq),
        featureAddress(kingSq, side, dp.add0.pc, dp.add0.sq),
        featureAddress(kingSq, side, dp.sub1.pc, dp.sub1.sq),
        featureAddress(kingSq, side, dp.add1.pc, dp.add1.sq));
    } else if (dp.type == DirtyPieces::CAPTURE) 
    { 
      multiSubAddSub<L1>((VecI*) colors[side], (VecI*) input.colors[side], 
        featureAddress(kingSq, side, dp.sub0.pc, dp.sub0.sq),
        featureAddress(kingSq, side, dp.add0.pc, dp.add0.sq),
        featureAddress(kingSq, side, dp.sub1.pc, dp.sub1.sq));
    } else
    {
      multiSubAdd<L1>((VecI*) colors[side], (VecI*) input.colors[side], 
        featureAddress(kingSq, side, dp.sub0.pc, dp.sub0.sq),
        featureAddress(kingSq, side, dp.add0.pc, dp.add0.sq));
    }
    updated[side] = true;
  }

  void Accumulator::reset(Color side) {
    if (useMaterialFallback) {
      memset(colors[side], 0, sizeof(colors[side]));
      updated[side] = true;
      return;
    }
    memcpy(colors[side], Weights->FeatureBiases, sizeof(Weights->FeatureBiases));
  }

  void Accumulator::refresh(Position& pos, Color side) {
    if (useMaterialFallback) {
      memset(colors[side], 0, sizeof(colors[side]));
      updated[side] = true;
      return;
    }
    reset(side);
    const Square kingSq = pos.kingSquare(side);
    Bitboard occupied = pos.pieces();
    while (occupied) {
      const Square sq = popLsb(occupied);
      addPiece(kingSq, side, pos.board[sq], sq);
    }
    updated[side] = true;
  }

  void FinnyEntry::reset() {
    memset(byColorBB, 0, sizeof(byColorBB));
    memset(byPieceBB, 0, sizeof(byPieceBB));
    acc.reset(WHITE);
    acc.reset(BLACK);
  }

  void loadWeights() {
    auto tryLoad = [&](const std::filesystem::path& candidate, bool verbose) {
      if (candidate.empty())
        return false;
      std::vector<uint8_t> buffer;
      if (!readBinaryFile(candidate, buffer)) {
        if (verbose) {
          std::string pathStr = candidate.generic_string();
          std::fprintf(stderr, "info string NNUE: failed to read %s\n", pathStr.c_str());
        }
        return false;
      }
      if (!initializeNetworkFromBuffer(buffer.data(), buffer.size())) {
        if (verbose) {
          std::string pathStr = candidate.generic_string();
          std::fprintf(stderr, "info string NNUE: invalid network %s (expected %zu bytes)\n",
                       pathStr.c_str(), sizeof(Net));
        }
        return false;
      }
      std::string pathStr = candidate.generic_string();
      std::fprintf(stderr, "info string NNUE: loaded network from %s\n", pathStr.c_str());
      return true;
    };

    bool loaded = false;
    std::filesystem::path exeDir = executableDirectory();

    auto evalOptionIt = UCI::Options.find("EvalFile");
    std::string evalOption = (evalOptionIt != UCI::Options.end()) ? (std::string) evalOptionIt->second : std::string();

    if (!evalOption.empty())
      loaded = tryLoad(evalOption, true);

    if (!loaded) {
      std::vector<std::filesystem::path> candidates;
      candidates.emplace_back(EvalFile);
      if (!exeDir.empty()) {
        candidates.emplace_back(exeDir / EvalFile);
        candidates.emplace_back(exeDir / "resources" / EvalFile);
      }
      for (const auto& candidate : candidates) {
        if (tryLoad(candidate, false)) {
          loaded = true;
          break;
        }
      }
    }

    if (!loaded && !exeDir.empty()) {
      if (auto found = findNetworkInDirectory(exeDir / "resources"))
        loaded = tryLoad(*found, false);
    }

    if (!loaded && !exeDir.empty()) {
      if (auto found = findNetworkInDirectory(exeDir))
        loaded = tryLoad(*found, false);
    }

    if (!loaded && initializeNetworkFromBuffer(gEmbeddedNNUEData, gEmbeddedNNUESize)) {
      std::fprintf(stderr, "info string NNUE: using embedded network (%s)\n", EvalFile);
      loaded = true;
    }

    if (!loaded) {
      if (Weights && !useMaterialFallback) {
        std::fprintf(stderr, "info string NNUE: keeping previously loaded network.\n");
        return;
      }
      if (Weights) {
        Util::freeAlign(Weights);
        Weights = nullptr;
      }
      useMaterialFallback = true;
      std::fprintf(stderr, "info string NNUE: no network found; using material eval.\n");
    }
  }

  Score evaluate(Position& pos, Accumulator& accumulator) {

    if (useMaterialFallback)
      return materialOnlyEval(pos);

    constexpr int divisor = (32 + OutputBuckets - 1) / OutputBuckets;
    int bucket = (BitCount(pos.pieces()) - 2) / divisor;

    __m128i base = _mm_setzero_si128();
    __m128i lookupInc = _mm_set1_epi16(8);

    VecF vecfZero = setzeroPs();
    VecF vecfOne = set1Ps(1.0f);
    VecI veciZero = setzeroSi();
    VecI veciOne = set1Epi16(NetworkQA);

    // L1 propagation is int8 -> float, so we multiply 4 ft outputs at a time
    uint16_t nnzIndexes[L1 / 4];
    int nnzCount = 0;

    alignas(Alignment) uint8_t ftOut[L1];
    alignas(Alignment) float l1Out[L2 * 2];
    alignas(Alignment) float l2Out[L3];
    float l3Out;

    constexpr float L1Mul = 1.0f / float(NetworkQA * NetworkQA * NetworkQB >> FtShift);
    VecF L1MulVec = set1Ps(L1Mul);

    // activate FT
    for (int them = 0; them <= 1; ++them) 
    {
      int16_t* acc = accumulator.colors[pos.sideToMove ^ them];
      for (int i = 0; i < L1 / 2; i += I8InVec) 
      {
        VecI c0 = minEpi16(maxEpi16(AsVecI(acc[i]), veciZero), veciOne);
        VecI c1 = minEpi16(AsVecI(acc[i + L1/2]), veciOne);

        VecI d0 = minEpi16(maxEpi16(AsVecI(acc[i + I16InVec]), veciZero), veciOne);
        VecI d1 = minEpi16(AsVecI(acc[i + L1/2 + I16InVec]), veciOne);

        VecI cProd = mulhiEpi16(slliEpi16(c0, 16 - FtShift), c1);
        VecI dProd = mulhiEpi16(slliEpi16(d0, 16 - FtShift), d1);

        VecI packed = packusEpi16(cProd, dProd);
        AsVecI(ftOut[them * L1 / 2 + i]) = packed;

#if defined(__AVX2__)
        // a bit mask where each bit (x) is 1, if the xth int32 in the product is > 0
        uint16_t nnzMask = getNnzMask(packed);

        // Usually (in AVX2) only one lookup is needed, as there are 8 ints in a vec.
        for (int lookup = 0; lookup < FloatInVec; lookup += 8) {
          uint8_t slice = (nnzMask >> lookup) & 0xFF;
          __m128i indexes = _mm_loadu_si128((__m128i*)nnzTable[slice]);
          _mm_storeu_si128((__m128i*)(nnzIndexes + nnzCount), _mm_add_epi16(base, indexes));
          nnzCount += BitCount(slice);
          base = _mm_add_epi16(base, lookupInc);
        }
#endif
      }
    }

#if !defined(__AVX2__) // if we are in SSSE3
    for (int i = 0; i < L1; i += 2 * I8InVec) {
      // a bit mask where each bit (x) is 1, if the xth int32 in the product is > 0
      uint16_t nnzMask = getNnzMask(AsVecI(ftOut[i]));
      nnzMask |= getNnzMask(AsVecI(ftOut[i + I8InVec])) << 4;

      // Usually (in AVX2) only one lookup is needed, as there are 8 ints in a vec.
      uint8_t slice = nnzMask & 0xFF;
      __m128i indexes = _mm_loadu_si128((__m128i*)nnzTable[slice]);
      _mm_storeu_si128((__m128i*)(nnzIndexes + nnzCount), _mm_add_epi16(base, indexes));
      nnzCount += BitCount(slice);
      base = _mm_add_epi16(base, lookupInc);
    }
#endif

    { // propagate l1

      alignas(Alignment) int32_t sums[L2];
      memset(sums, 0, sizeof(sums));

      for (int i = 0; i < nnzCount; i++) {
        int l1in = nnzIndexes[i]*4;
        VecI vecFtOut = set1Epi32( *(uint32_t*)(ftOut + l1in) );
        for (int j = 0; j < L2; j += FloatInVec) {
          VecI vecWeight = AsVecI(Weights->L1Weights[bucket][l1in + j/4]);
          AsVecI(sums[j]) = dpbusdEpi32(AsVecI(sums[j]), vecFtOut, vecWeight);
        }
      }

      for (int i = 0; i < L2; i += FloatInVec) {
        VecF vecBias = AsVecF(Weights->L1Biases[bucket][i]);
        VecF prod = mulAddPs(castEpi32ToPs(AsVecI(sums[i])), L1MulVec, vecBias);
        VecF squared = mulPs(prod, prod);

        AsVecF(l1Out[i]) = minPs(maxPs(prod, vecfZero), vecfOne);
        AsVecF(l1Out[i + L2]) = minPs(squared, vecfOne);
      }
    }

    constexpr int Chunks = 64 / sizeof(VecF);

    { // propagate l2
      alignas(Alignment) float sums[L3];
      memcpy(sums, Weights->L2Biases[bucket], sizeof(sums));

      for (int i = 0; i < L2 * 2; ++i) {
        VecF vecL1Out = set1Ps(l1Out[i]);
        for (int j = 0; j < L3; j += FloatInVec)
          AsVecF(sums[j]) = mulAddPs(AsVecF(Weights->L2Weights[bucket][i][j]), vecL1Out, AsVecF(sums[j]));
      }

      for (int i = 0; i < L3; i += FloatInVec)
        AsVecF(l2Out[i]) = minPs(maxPs(AsVecF(sums[i]), vecfZero), vecfOne);
    }

    { // propagate l3
      VecF sums[Chunks];
      for (int j = 0; j < Chunks; j++)
        sums[j] = vecfZero;
      for (int i = 0; i < L3; i += FloatInVec * Chunks) {
        for (int j = 0; j < Chunks; j++)
          sums[j] = mulAddPs(AsVecF(l2Out[i + j * FloatInVec]), AsVecF( Weights->L3Weights[bucket][i + j * FloatInVec]), sums[j]);
      }

      VecF totalSum = sums[0];
      for (int j = 1; j < Chunks; j++)
        totalSum = addPs(totalSum, sums[j]);
      
      l3Out = Weights->L3Biases[bucket] + reduceAddPs(totalSum);
    }

    return l3Out * NetworkScale;
  }

}
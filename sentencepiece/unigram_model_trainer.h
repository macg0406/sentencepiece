// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.!

#ifndef UNIGRAM_MODEL_TRAINER_H_
#define UNIGRAM_MODEL_TRAINER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "sentencepiece/sentencepiece_model.pb.h"
#include "absl/strings/string_view.h"
#include "sentencepiece/trainer_interface.h"
#include "sentencepiece/unigram_model.h"
#include "sentencepiece/util.h"

namespace sentencepiece {
namespace unigram {

using string_util::UnicodeText;

class TrainerModel : public ModelBase {
 public:
  using SentencePieces = std::vector<std::pair<std::string, float>>;

  TrainerModel() = delete;
  TrainerModel(const TrainerSpec &trainer_spec,
               const NormalizerSpec &normalizaiton_spec);
  ~TrainerModel() override;

  // Returns the sentencepieces.
  // The meta symbols, e.g., </s> are NOT included.
  const SentencePieces &GetSentencePieces() const;

  // Sets sentencepieces. The sentencepieces are moved.
  // The meta symbols, e.g., </s> are NOT included.
  void SetSentencePieces(SentencePieces &&sentencepieces);

  int GetPieceSize() const override { return sentencepieces_.size(); }

  float GetScore(int index) const override {
    return sentencepieces_[index].second;
  }

  std::string IdToPiece(int id) const override {
    return sentencepieces_[id].first;
  }

  bool IsControl(int id) const override { return false; }

  bool IsUnknown(int id) const override { return false; }

  bool IsUnused(int id) const override { return false; }

  bool IsUserDefined(int id) const override { return false; }

  EncodeResult Encode(absl::string_view normalized) const override { return {}; }

 private:
  SentencePieces sentencepieces_;
  TrainerSpec trainer_spec_;
  NormalizerSpec normalizer_spec_;
};

class Trainer : public TrainerInterface {
 public:
  Trainer(const TrainerSpec &trainer_spec,
          const NormalizerSpec &normalizer_spec)
      : TrainerInterface::TrainerInterface(trainer_spec, normalizer_spec) {}

  util::Status Train() override;

 protected:
    // Returns true if |piece| is valid sentence piece.
  // The result is affected by
  // max_sentencepiece_length, split_by_whiespace, split_by_unicode_script.
  bool IsValidSentencePiece(const UnicodeText &piece) const override;

 private:
  FRIEND_TEST(TrainerTest, IsValidSentencePieceTest);

  // Makes seed pieces from the training corpus.
  // The size of seed pieces is determined by seed_sentencepiece_size.
  TrainerModel::SentencePieces MakeSeedSentencePieces() const;

  // Executes the E step of EM and returns expected count.
  // The index of return array is the vocab id.
  // |objective| is a negative likelihood of the current model.
  // |num_token| is the number of total tokens to tokenize
  // training corpus.
  std::vector<float> RunEStep(const TrainerModel &model, float *objective,
                              int64 *num_tokens) const;

  // Executes the M step of EM with the expected frequency and
  // returns new pieces.
  TrainerModel::SentencePieces RunMStep(
      const TrainerModel &model, const std::vector<float> &expected) const;

  // Heuristically prunes the current pieces.
  // This is called after each EM sub-iteration.
  TrainerModel::SentencePieces PruneSentencePieces(
      const TrainerModel &model) const;

  // Makes the final sentence pieces by incorporating the required characters
  // and control/user defined symbols.
  TrainerModel::SentencePieces FinalizeSentencePieces(
      const TrainerModel &model) const;

  // When the size of SentencePieces becomes less than desired_vocab_size_,
  // break the main training loop. desired_vocab_size_ = 1.1 * vocab_size_
  // for now.
  int desired_vocab_size_;
};
}  // namespace unigram
}  // namespace sentencepiece
#endif  // UNIGRAM_MODEL_TRAINER_H_

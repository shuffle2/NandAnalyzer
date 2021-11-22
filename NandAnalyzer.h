#pragma once

#include <Analyzer.h>
#include <AnalyzerChannelData.h>
#include <AnalyzerHelpers.h>
#include <AnalyzerResults.h>
#include <AnalyzerSettings.h>
#include <array>
#include <memory>
#include <optional>

struct NandChannels {
  NandChannels() {
    CE_n = UNDEFINED_CHANNEL;
    CLE = UNDEFINED_CHANNEL;
    ALE = UNDEFINED_CHANNEL;
    WE_n = UNDEFINED_CHANNEL;
    DQS = UNDEFINED_CHANNEL;
    for (auto& c : DQ) {
      c = UNDEFINED_CHANNEL;
    }
  }
  Channel CE_n;
  Channel CLE;
  Channel ALE;
  Channel WE_n;
  Channel DQS;
  static constexpr size_t DATA_WIDTH{8};
  std::array<Channel, DATA_WIDTH> DQ;
};

SimpleArchive& operator<<(SimpleArchive& lhs, NandChannels& rhs) {
  lhs << rhs.CE_n;
  lhs << rhs.CLE;
  lhs << rhs.ALE;
  lhs << rhs.WE_n;
  lhs << rhs.DQS;
  for (auto& c : rhs.DQ) {
    lhs << c;
  }
  return lhs;
}

SimpleArchive& operator>>(SimpleArchive& lhs, NandChannels& rhs) {
  lhs >> rhs.CE_n;
  lhs >> rhs.CLE;
  lhs >> rhs.ALE;
  lhs >> rhs.WE_n;
  lhs >> rhs.DQS;
  for (auto& c : rhs.DQ) {
    lhs >> c;
  }
  return lhs;
}

struct NandUiChannels {
  AnalyzerSettingInterfaceChannel CE_n;
  AnalyzerSettingInterfaceChannel CLE;
  AnalyzerSettingInterfaceChannel ALE;
  AnalyzerSettingInterfaceChannel WE_n;
  AnalyzerSettingInterfaceChannel DQS;
  std::array<AnalyzerSettingInterfaceChannel, NandChannels::DATA_WIDTH> DQ;
};

class NandAnalyzerSettings : public AnalyzerSettings {
 public:
  NandAnalyzerSettings();
  // Get the settings out of the interfaces, validate them, and save them to
  // your local settings vars.
  virtual bool SetSettingsFromInterfaces() final;
  // Load your settings from the provided string
  virtual void LoadSettings(const char* settings) final;
  // Save your settings to a string and return it. (use SetSettingsString,
  // return GetSettingsString)
  virtual const char* SaveSettings() final;

  NandChannels channels_;
  NandUiChannels ui_channels_;
};

class NandAnalyzerResults : public AnalyzerResults {
  virtual void GenerateBubbleText(U64 frame_index,
                                  Channel& channel,
                                  DisplayBase display_base) final;
  virtual void GenerateExportFile(const char* file,
                                  DisplayBase display_base,
                                  U32 export_type_user_id) final;
  virtual void GenerateFrameTabularText(U64 frame_index,
                                        DisplayBase display_base) final;
  virtual void GeneratePacketTabularText(U64 packet_id,
                                         DisplayBase display_base) final;
  virtual void GenerateTransactionTabularText(U64 transaction_id,
                                              DisplayBase display_base) final;
};

struct NandAnalyzerChannels {
  AnalyzerChannelData* CE_n{};
  AnalyzerChannelData* CLE{};
  AnalyzerChannelData* ALE{};
  AnalyzerChannelData* WE_n{};
  AnalyzerChannelData* DQS{};
  std::array<AnalyzerChannelData*, NandChannels::DATA_WIDTH> DQ{};
};

class NandAnalyzer : public Analyzer2 {
 public:
  NandAnalyzer();
  virtual ~NandAnalyzer() final;
  virtual void WorkerThread() final;

  // sample_rate: if there are multiple devices attached, and one is faster than
  // the other, we can sample at the speed of the faster one; and pretend the
  // slower one is the same speed.
  virtual U32 GenerateSimulationData(
      U64 newest_sample_requested,
      U32 sample_rate,
      SimulationChannelDescriptor** simulation_channels) final {
    return 0;
  }
  // provide the sample rate required to generate good simulation data
  virtual U32 GetMinimumSampleRateHz() final { return 0; }
  virtual const char* GetAnalyzerName() const final { return name_; }
  virtual bool NeedsRerun() final { return false; }

  virtual void SetupResults() final;

  static constexpr const char* name_{"NAND"};
  NandAnalyzerSettings settings_;
  NandAnalyzerResults results_;
  NandAnalyzerChannels channels_;
};

extern "C" {
ANALYZER_EXPORT const char* __cdecl GetAnalyzerName() {
  return NandAnalyzer::name_;
}
ANALYZER_EXPORT Analyzer* __cdecl CreateAnalyzer() {
  return new NandAnalyzer();
}
ANALYZER_EXPORT void __cdecl DestroyAnalyzer(Analyzer* analyzer) {
  delete analyzer;
}
}

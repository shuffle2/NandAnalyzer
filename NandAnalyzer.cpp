#include "NandAnalyzer.h"
#include <AnalyzerHelpers.h>
#include <format>
#include <fstream>

#define CREATE_CHANNEL_SETTING(name, desc)             \
  do {                                                 \
    ui_channels_.name.SetTitleAndTooltip(#name, desc); \
    ui_channels_.name.SetChannel(channels_.name);      \
    AddInterface(&ui_channels_.name);                  \
    AddChannel(channels_.name, #name, false);          \
  } while (0)

NandAnalyzerSettings::NandAnalyzerSettings() {
  ClearChannels();

  CREATE_CHANNEL_SETTING(CE_n, "Chip Enable");
  CREATE_CHANNEL_SETTING(CLE, "Command Latch Enable");
  CREATE_CHANNEL_SETTING(ALE, "Address Latch Enable");
  CREATE_CHANNEL_SETTING(WE_n, "Write Enable");
  CREATE_CHANNEL_SETTING(DQS, "Data Strobe");

  for (size_t i = 0; i < ui_channels_.DQ.size(); i++) {
    auto& ui = ui_channels_.DQ[i];
    auto& c = channels_.DQ[i];
    auto name = std::format("DQ[{}]", i);
    ui.SetTitleAndTooltip(name.c_str(), "Data");
    ui.SetChannel(channels_.DQ[i]);
    AddInterface(&ui);
    AddChannel(c, name.c_str(), false);
  }
}

#undef CREATE_CHANNEL_SETTING

bool NandAnalyzerSettings::SetSettingsFromInterfaces() {
  std::array<Channel, 5 + NandChannels::DATA_WIDTH> tmp_channels;
  tmp_channels[0] = ui_channels_.CE_n.GetChannel();
  tmp_channels[1] = ui_channels_.CLE.GetChannel();
  tmp_channels[2] = ui_channels_.ALE.GetChannel();
  tmp_channels[3] = ui_channels_.WE_n.GetChannel();
  tmp_channels[4] = ui_channels_.DQS.GetChannel();
  for (size_t i = 0; i < ui_channels_.DQ.size(); i++) {
    tmp_channels[5 + i] = ui_channels_.DQ[i].GetChannel();
  }
  if (AnalyzerHelpers::DoChannelsOverlap(&tmp_channels[0],
                                         tmp_channels.size())) {
    SetErrorText("Please select different channels for each input.");
    return false;
  }

  channels_.CE_n = ui_channels_.CE_n.GetChannel();
  channels_.CLE = ui_channels_.CLE.GetChannel();
  channels_.ALE = ui_channels_.ALE.GetChannel();
  channels_.WE_n = ui_channels_.WE_n.GetChannel();
  channels_.DQS = ui_channels_.DQS.GetChannel();
  for (size_t i = 0; i < channels_.DQ.size(); i++) {
    auto& c = channels_.DQ[i];
    c = ui_channels_.DQ[i].GetChannel();
  }

  ClearChannels();
  AddChannel(channels_.CE_n, "CE_n", channels_.CE_n != UNDEFINED_CHANNEL);
  AddChannel(channels_.CLE, "CLE", channels_.CLE != UNDEFINED_CHANNEL);
  AddChannel(channels_.ALE, "ALE", channels_.ALE != UNDEFINED_CHANNEL);
  AddChannel(channels_.WE_n, "WE_n", channels_.WE_n != UNDEFINED_CHANNEL);
  AddChannel(channels_.DQS, "DQS", channels_.DQS != UNDEFINED_CHANNEL);
  for (size_t i = 0; i < channels_.DQ.size(); i++) {
    auto& c = channels_.DQ[i];
    auto name = std::format("DQ[{}]", i);
    AddChannel(c, name.c_str(), c != UNDEFINED_CHANNEL);
  }

  return true;
}

void NandAnalyzerSettings::LoadSettings(const char* settings) {
  SimpleArchive archive;
  archive.SetString(settings);
  archive >> channels_;

  ClearChannels();
  AddChannel(channels_.CE_n, "CE_n", channels_.CE_n != UNDEFINED_CHANNEL);
  AddChannel(channels_.CLE, "CLE", channels_.CLE != UNDEFINED_CHANNEL);
  AddChannel(channels_.ALE, "ALE", channels_.ALE != UNDEFINED_CHANNEL);
  AddChannel(channels_.WE_n, "WE_n", channels_.WE_n != UNDEFINED_CHANNEL);
  AddChannel(channels_.DQS, "DQS", channels_.DQS != UNDEFINED_CHANNEL);
  for (size_t i = 0; i < channels_.DQ.size(); i++) {
    auto& c = channels_.DQ[i];
    auto name = std::format("DQ[{}]", i);
    AddChannel(c, name.c_str(), c != UNDEFINED_CHANNEL);
  }

  ui_channels_.CE_n.SetChannel(channels_.CE_n);
  ui_channels_.CLE.SetChannel(channels_.CLE);
  ui_channels_.ALE.SetChannel(channels_.ALE);
  ui_channels_.WE_n.SetChannel(channels_.WE_n);
  ui_channels_.DQS.SetChannel(channels_.DQS);
  for (size_t i = 0; i < channels_.DQ.size(); i++) {
    auto& c = channels_.DQ[i];
    ui_channels_.DQ[i].SetChannel(c);
  }
}

const char* NandAnalyzerSettings::SaveSettings() {
  SimpleArchive archive;
  archive << channels_;
  return SetReturnString(archive.GetString());
}

void NandAnalyzerResults::GenerateBubbleText(U64 frame_index,
                                             Channel& channel,
                                             DisplayBase display_base) {
  ClearResultStrings();
}

void NandAnalyzerResults::GenerateExportFile(const char* file,
                                             DisplayBase display_base,
                                             U32 export_type_user_id) {}

void NandAnalyzerResults::GenerateFrameTabularText(U64 frame_index,
                                                   DisplayBase display_base) {}

// Never seems to be called :/
void NandAnalyzerResults::GeneratePacketTabularText(U64 packet_id,
                                                    DisplayBase display_base) {}
void NandAnalyzerResults::GenerateTransactionTabularText(
    U64 transaction_id,
    DisplayBase display_base) {}

NandAnalyzer::NandAnalyzer() {
  SetAnalyzerSettings(&settings_);
}

NandAnalyzer::~NandAnalyzer() {
  KillThread();
}

void NandAnalyzer::WorkerThread() {
  auto& ce_n = channels_.CE_n;
  auto& cle = channels_.CLE;

  ce_n = GetAnalyzerChannelData(settings_.channels_.CE_n);
  cle = GetAnalyzerChannelData(settings_.channels_.CLE);
  channels_.ALE = GetAnalyzerChannelData(settings_.channels_.ALE);
  channels_.WE_n = GetAnalyzerChannelData(settings_.channels_.WE_n);
  channels_.DQS = GetAnalyzerChannelData(settings_.channels_.DQS);
  for (size_t i = 0; i < settings_.channels_.DQ.size(); i++) {
    channels_.DQ[i] = GetAnalyzerChannelData(settings_.channels_.DQ[i]);
  }

  // a cycle begins with CE_n falling edge
  // CLE, ALE, or both must have rising edge after at least one clock on WE_n
  while (true) {
    // ReportProgress(lck->GetSampleNumber());

    // results_.AddMarker(lck->GetSampleNumber(),
    //                   AnalyzerResults::MarkerType::Stop,
    //                   settings_.channels_.LFRAMEn);
    // why doesn't this generate a packet :(
    results_.CommitPacketAndStartNewPacket();
    results_.CommitResults();
  }
}

void NandAnalyzer::SetupResults() {
  SetAnalyzerResults(&results_);
  results_.AddChannelBubblesWillAppearOn(settings_.channels_.CE_n);
}

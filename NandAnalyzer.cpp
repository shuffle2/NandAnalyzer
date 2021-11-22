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
    auto name = std::format("DQ{}", i);
    ui.SetTitleAndTooltip(name.c_str(), "Data");
    ui.SetChannel(channels_.DQ[i]);
    ui.SetSelectionOfNoneIsAllowed(true);
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
  bool has_dq = false;
  for (size_t i = 0; i < ui_channels_.DQ.size(); i++) {
    auto c = ui_channels_.DQ[i].GetChannel();
    tmp_channels[5 + i] = c;
    if (!has_dq && c != UNDEFINED_CHANNEL) {
      has_dq = true;
    }
  }
  if (AnalyzerHelpers::DoChannelsOverlap(&tmp_channels[0],
                                         tmp_channels.size())) {
    SetErrorText("Please select different channels for each input.");
    return false;
  }
  if (!has_dq) {
    SetErrorText("Please select at least 1 DQ channel.");
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
    auto name = std::format("DQ{}", i);
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
    auto name = std::format("DQ{}", i);
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

  Frame f = GetFrame(frame_index);
  std::string text;

  const auto ft = (NandAnalyzer::FrameType)f.mType;
  const auto data = (U8)f.mData1;

  switch (ft) {
  case NandAnalyzer::FrameType::kEnvelope:
    // this is a fake type
    return;
  case NandAnalyzer::FrameType::kCommand:
    text = std::format("CMD:{:02x}", data);
    break;
  case NandAnalyzer::FrameType::kAddress:
    text = std::format("ADDR:{:02x}", data);
    break;
  case NandAnalyzer::FrameType::kData:
    text = std::format("DATA:{:02x}", data);
    break;
  }
  AddResultString(text.c_str());
}

void NandAnalyzerResults::GenerateExportFile(const char* file,
                                             DisplayBase display_base,
                                             U32 export_type_user_id) {
  std::ofstream file_stream(file, std::ios::out);

  struct Packet {
    std::optional<U8> cmd{};
    std::vector<U8> addr;
    std::vector<U8> data;
  } merged_packet;

  auto write_packet = [&file_stream](const Packet& packet) {
    if (packet.cmd.has_value()) {
      std::string line;
      line += std::format("CMD:{:02x}", packet.cmd.value());
      if (packet.addr.size()) {
        line += " ADDR:";
        for (const auto& addr : packet.addr) {
          line += std::format("{:02x}", addr);
        }
      }
      if (packet.data.size()) {
        line += " DATA:";
        for (const auto& val : packet.data) {
          line += std::format("{:02x}", val);
        }
      }
      file_stream << line << std::endl;
    }
  };

  const U64 num_frames = GetNumFrames();
  for (U64 frame_index = 0; frame_index < num_frames; frame_index++) {
    Frame f = GetFrame(frame_index);
    const auto ft = (NandAnalyzer::FrameType)f.mType;
    const auto data = (U8)f.mData1;

    switch (ft) {
    case NandAnalyzer::FrameType::kEnvelope:
      write_packet(merged_packet);
      merged_packet = {};
      break;
    case NandAnalyzer::FrameType::kCommand:
      merged_packet.cmd = data;
      break;
    case NandAnalyzer::FrameType::kAddress:
      merged_packet.addr.push_back(data);
      break;
    case NandAnalyzer::FrameType::kData:
      merged_packet.data.push_back(data);
      break;
    }

    if (UpdateExportProgressAndCheckForCancel(frame_index, num_frames)) {
      return;
    }
  }

  write_packet(merged_packet);
  UpdateExportProgressAndCheckForCancel(num_frames, num_frames);
}

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
  channels_.CE_n = GetAnalyzerChannelData(settings_.channels_.CE_n);
  channels_.CLE = GetAnalyzerChannelData(settings_.channels_.CLE);
  channels_.ALE = GetAnalyzerChannelData(settings_.channels_.ALE);
  channels_.WE_n = GetAnalyzerChannelData(settings_.channels_.WE_n);
  channels_.DQS = GetAnalyzerChannelData(settings_.channels_.DQS);
  for (size_t i = 0; i < settings_.channels_.DQ.size(); i++) {
    auto& c = settings_.channels_.DQ[i];
    if (c == UNDEFINED_CHANNEL) {
      // leave undefined DQ as nullptr
      continue;
    }
    channels_.DQ[i] = GetAnalyzerChannelData(c);
  }

  // a cycle begins with CE_n falling edge
  // WE_n rising edge then clocks status of CLE and ALE
  // DQ is clocked by DQS *centered* (for DDR)
  auto& ce_n = channels_.CE_n;
  auto& cle = channels_.CLE;
  auto& ale = channels_.ALE;
  auto& we_n = channels_.WE_n;
  auto& dqs = channels_.DQS;
  while (true) {
    ReportProgress(ce_n->GetSampleNumber());

    // Sync CE_n and WE_n when CE_n is low/falling
    if (ce_n->GetBitState() == BIT_HIGH) {
      ce_n->AdvanceToNextEdge();
    }
    const auto ce_n_f = ce_n->GetSampleNumber();
    we_n->AdvanceToAbsPosition(ce_n_f);
    // Advance CE_n to rising
    ce_n->AdvanceToNextEdge();
    const auto ce_n_r = ce_n->GetSampleNumber();

    bool has_cmd = false;

    while (true) {
      const auto we_n_next = we_n->GetSampleOfNextEdge();
      const auto dqs_next = dqs->GetSampleOfNextEdge();

      const auto first_edge =
          has_cmd ? std::min(we_n_next, dqs_next) : we_n_next;

      if (first_edge >= ce_n_r) {
        break;
      }

      // Be careful to give precedence to cmd/addr cycles
      we_n->AdvanceToAbsPosition(first_edge);
      dqs->AdvanceToAbsPosition(first_edge);
      const bool data_cycle =
          (first_edge != we_n_next) && (we_n->GetBitState() == BIT_HIGH);

      if (data_cycle) {
        // TODO in some cases, DQ should be centered, not edge
        U8 data = SyncAndReadDQ(first_edge);

        // this is extremely fragile for some reason...Logic likes getting mad
        U64 end = ce_n_r - 1;
        if (dqs->DoMoreTransitionsExistInCurrentData()) {
          end = std::min(end, dqs->GetSampleOfNextEdge());
        }
        AddFrame(kData, first_edge, end, data);

        auto marker = (dqs->GetBitState() == BIT_HIGH)
                          ? AnalyzerResults::MarkerType::UpArrow
                          : AnalyzerResults::MarkerType::DownArrow;
        results_.AddMarker(first_edge, marker, settings_.channels_.DQS);
      } else if (we_n->GetBitState() == BIT_HIGH) {
        // WE_n rising
        const auto we_n_r = we_n->GetSampleNumber();
        results_.AddMarker(we_n_r, AnalyzerResults::MarkerType::UpArrow,
                           settings_.channels_.WE_n);

        cle->AdvanceToAbsPosition(we_n_r);
        ale->AdvanceToAbsPosition(we_n_r);

        FrameType frame_type = kInvalid;
        U64 end{};
        if (cle->GetBitState() == BIT_HIGH) {
          has_cmd = true;
          frame_type = kCommand;
          end = cle->GetSampleOfNextEdge() - 1;
        } else if (ale->GetBitState() == BIT_HIGH) {
          frame_type = kAddress;
          end = std::min(we_n->GetSampleOfNextEdge(),
                         ale->GetSampleOfNextEdge()) -
                1;
        } else {
          results_.AddMarker(first_edge, AnalyzerResults::MarkerType::ErrorDot,
                             settings_.channels_.CE_n);
        }
        if (frame_type != kInvalid) {
          U8 data = SyncAndReadDQ(we_n_r);

          AddFrame(frame_type, we_n_r, end, data);
        }
      }
    }

    AddFrame(kEnvelope, ce_n_f, ce_n_r);
    results_.AddMarker(ce_n_r, AnalyzerResults::MarkerType::Stop,
                       settings_.channels_.CE_n);
    // why doesn't this generate a packet :(
    results_.CommitPacketAndStartNewPacket();
    results_.CommitResults();
  }
}

void NandAnalyzer::SetupResults() {
  SetAnalyzerResults(&results_);
  results_.AddChannelBubblesWillAppearOn(settings_.channels_.CE_n);
}

U8 NandAnalyzer::SyncAndReadDQ(U64 sample_number) {
  U8 data = 0;
  for (size_t i = 0; i < channels_.DQ.size(); i++) {
    auto& c = channels_.DQ[i];
    if (!c) {
      continue;
    }
    c->AdvanceToAbsPosition(sample_number);
    U8 b = (c->GetBitState() == BIT_HIGH) ? 1 : 0;
    data |= b << i;
  }
  return data;
}

bool NandAnalyzer::AddFrame(FrameType frame_type,
                            U64 start,
                            U64 end,
                            U64 data1,
                            U64 data2,
                            U8 flags) {
  // NOTE: end - start must be > 0 or Logic crashes when trying to zoom to the
  // frame
  if (start >= end) {
    return false;
  }
  Frame frame{};
  frame.mStartingSampleInclusive = start;
  frame.mEndingSampleInclusive = end;
  frame.mType = frame_type;
  frame.mData1 = data1;
  frame.mData2 = data2;
  frame.mFlags = flags;
  results_.AddFrame(frame);
  ReportProgress(frame.mEndingSampleInclusive);
  return true;
}

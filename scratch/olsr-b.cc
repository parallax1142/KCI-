#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "olsr-routing-protocol-original.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"

#include <random>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleOlsrExample");

#define ENABLE_INTERMEDIATE_LOGGING

void IntermediateRxCallback(Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface)
{
#ifdef ENABLE_INTERMEDIATE_LOGGING
    NS_LOG_UNCOND("Intermediate node received one packet on interface " << interface << "!");
#endif
}

/* void PrintNodePositions(NodeContainer c)
{
    for (uint32_t i = 0; i < c.GetN(); ++i)
    {
        Ptr<MobilityModel> mobility = c.Get(i)->GetObject<MobilityModel>();
        Vector pos = mobility->GetPosition();
        NS_LOG_UNCOND("Node " << i << " position: x=" << pos.x << ", y=" << pos.y << ", z=" << pos.z);
    }
    Simulator::Schedule(Seconds(5.0), &PrintNodePositions, c);
} */

int main(int argc, char *argv[])
{
    LogComponentEnable("SimpleOlsrExample", LOG_LEVEL_INFO);
    LogComponentDisable("PacketSink", LOG_LEVEL_INFO); // PacketSink 로깅 활성화
    LogComponentDisable("OnOffApplication", LOG_LEVEL_INFO); // OnOff 로깅 비활성화

    std::string phyMode("DsssRate1Mbps");

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    std::random_device rd; // 랜덤 시드 설정
    RngSeedManager::SetSeed (rd());
    

    NS_LOG_INFO("Create nodes.");
    NodeContainer c;
    c.Create(50); 

    // Set up WiFi devices and channel
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    // 전파 손실 측정 // 로그 거리 전파 손실 모델 // 송신점에서의 거리의 로그에 비례하여 감소 // Exponent 경로 손실 지수(신호 감쇠 정도)
    wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel", "Exponent", DoubleValue(3.0));  // 일반적인 실외 환경 2~4

    YansWifiPhyHelper wifiPhy;
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::IdealWifiManager"); // Wi-Fi 속도 제어 // 이상적인 조건(채널 상태 정보 완벽) IdealWifiManager


    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, c);

    // Enable OLSR
    NS_LOG_INFO("Enabling OLSR Routing.");
    OlsrHelper olsr;

    Ipv4StaticRoutingHelper staticRouting;

    Ipv4ListRoutingHelper list;
    list.Add(staticRouting, 0);
    list.Add(olsr, 10);

    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(c);

    // Assign IP addresses
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(devices);

    // Set up mobility model
    MobilityHelper mobility;
    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
    x->SetAttribute("Min", DoubleValue(0.0));
    x->SetAttribute("Max", DoubleValue(100.0));
    Ptr<UniformRandomVariable> y = CreateObject<UniformRandomVariable>();
    y->SetAttribute("Min", DoubleValue(0.0));
    y->SetAttribute("Max", DoubleValue(100.0));

    Ptr<RandomRectanglePositionAllocator> positionAlloc = CreateObject<RandomRectanglePositionAllocator>();
    positionAlloc->SetX(x);
    positionAlloc->SetY(y);
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=10.0]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                            "PositionAllocator", PointerValue(positionAlloc));
    mobility.Install(c);

    // Schedule position logging
    // Simulator::Schedule(Seconds(21.0), &PrintNodePositions, c);

    // Create an OnOff application to send UDP datagrams from n0 to n2
    NS_LOG_INFO("Create Applications.");
    uint16_t port = 9; // Discard port (RFC 863)

    // 노드 인덱스 벡터 생성 (0부터 n까지)
    std::vector<uint32_t> nodeIndices(c.GetN());
    std::iota(nodeIndices.begin(), nodeIndices.end(), 0);

    // 노드 인덱스를 섞습니다
    std::mt19937 g(rd());
    std::shuffle(nodeIndices.begin(), nodeIndices.end(), g);

    // 소스 노드와 목적지 노드를 선택합니다
    std::vector<uint32_t> sourceNodes(nodeIndices.begin(), nodeIndices.begin() + 6);  // 0 ~ n
    std::vector<uint32_t> destNodes(nodeIndices.begin() + 6, nodeIndices.begin() + 12);  // n ~ 2n

    // 각 소스 노드에 대해 OnOff 애플리케이션을 생성합니다
    for (size_t j = 0; j < sourceNodes.size(); ++j) {
        OnOffHelper onoff("ns3::UdpSocketFactory", 
                        InetSocketAddress(i.GetAddress(destNodes[j]), port));
        onoff.SetConstantRate(DataRate("80Kb/s"));
        onoff.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer sourceApp = onoff.Install(c.Get(sourceNodes[j]));
        sourceApp.Start(Seconds(1.0));
        sourceApp.Stop(Seconds(60.0));

        // 목적지 노드에 PacketSink 애플리케이션을 설치합니다
        PacketSinkHelper sink("ns3::UdpSocketFactory", 
                            InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(c.Get(destNodes[j]));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(61.0));

        NS_LOG_INFO("Source node: " << sourceNodes[j] << " -> Destination node: " << destNodes[j]);
    }

    // FlowMonitor 설치
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(30));
    

    NS_LOG_INFO("Run Simulation.");
    Simulator::Run();

        // 시뮬레이션 후 결과 분석
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double avgThroughput = 0.0;
    double avgDelay = 0.0;
    double avgPacketDeliveryRatio = 0.0;
    uint32_t flowCount = 0;
    uint32_t validDelayCount = 0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        // NS_LOG_UNCOND("\nFlow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
        // NS_LOG_UNCOND("Tx Packets = " << i->second.txPackets); // 전송된 패킷의 총 개수
        // NS_LOG_UNCOND("Rx Packets = " << i->second.rxPackets); // 수신된 패킷의 종 개수
        // NS_LOG_UNCOND("Tx Bytes = " << i->second.txBytes); // 전송된 데이터의 총 바이트 수
        // NS_LOG_UNCOND("Rx Bytes = " << i->second.rxBytes); // 수신된 데이터의 총 바이트 수
        double throughput = i->second.rxBytes * 8.0 / 20.0 / 1024 / 1024;
        // NS_LOG_UNCOND("Throughput = " << i->second.rxBytes * 8.0 / 20.0 / 1024 / 1024 << " Mbps"); // 단위 시간당 성공적으로 전달된 데이터의 양, 데이터 전송 능력
        
        double delay = 0;
        if (i->second.rxPackets > 0) {
            delay = i->second.delaySum.GetSeconds() / i->second.rxPackets;
            // NS_LOG_UNCOND("End-to-End Delay = " << delay << " s");
            if (!std::isnan(delay)) {
                avgDelay += delay;
                validDelayCount++;
            }
        } else {
            // NS_LOG_UNCOND("End-to-End Delay = N/A (no received packets)");
        }

        double pdr = 0;
        if (i->second.txPackets > 0) {
            pdr = (double)i->second.rxPackets / i->second.txPackets * 100;
            // NS_LOG_UNCOND("Packet Delivery Ratio = " << pdr << " %");
        } else {
            // NS_LOG_UNCOND("Packet Delivery Ratio = N/A (no transmitted packets)");
        }

        avgThroughput += throughput;
        avgPacketDeliveryRatio += pdr;
        flowCount++;
    }

    NS_LOG_UNCOND("\nAverage Results:");
    if (flowCount > 0) {
        avgThroughput /= flowCount;
        avgPacketDeliveryRatio /= flowCount;

        NS_LOG_UNCOND("Average Throughput = " << avgThroughput << " Mbps");
        NS_LOG_UNCOND("Average Packet Delivery Ratio = " << avgPacketDeliveryRatio << " %");
        
        if (validDelayCount > 0) {
            avgDelay /= validDelayCount;
            NS_LOG_UNCOND("Average End-to-End Delay = " << avgDelay << " s");
        } else {
            NS_LOG_UNCOND("Average End-to-End Delay = N/A (no valid delay measurements)");
        }
    }
    else {
        NS_LOG_UNCOND("No flows to calculate averages.\n");
    }

    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}
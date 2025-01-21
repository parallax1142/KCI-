/*
 * [기존 라이선스 및 헤더 주석]
 */

#include "ns3/aodv-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/ping-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/flow-monitor-module.h" // FlowMonitor 모듈 추가

#include <cmath>
#include <iostream>
#include <random>

using namespace ns3;

/**
 * \ingroup aodv-examples
 * \ingroup examples
 * \brief Test script with FlowMonitor.
 *
 * [기존 설명 유지]
 */

class AodvExample
{
  public:
    AodvExample();
    bool Configure(int argc, char** argv);
    void Run();
    void Report(std::ostream& os);

  private:
    // 기존 파라미터 유지
    uint32_t size;
    double step;
    double totalTime;
    bool pcap;
    bool printRoutes;

    // 기존 네트워크 구성 요소 유지
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;

    // FlowMonitor 관련 구성 요소 추가
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;

  private:
    // 기존 헬퍼 메소드 유지
    void CreateNodes();
    void CreateDevices();
    void InstallInternetStack();
    void InstallApplications();
    void InstallFlowMonitor(); // FlowMonitor 설치 헬퍼 메소드 추가
};

// 기존 main 함수 유지

int
main(int argc, char** argv)
{
    AodvExample test;
    if (!test.Configure(argc, argv))
    {
        NS_FATAL_ERROR("Configuration failed. Aborted.");
    }

    test.Run();
    test.Report(std::cout);
    return 0;
}

// 기존 생성자 유지

AodvExample::AodvExample()
    : size(20),
      step(50),
      totalTime(100),
      pcap(true),
      printRoutes(true)
{
}

// 기존 Configure 메소드 유지

bool
AodvExample::Configure(int argc, char** argv)
{
    // 기존 구성 유지
    // LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_ALL);

    std::random_device rd; // 랜덤 시드 설정
    RngSeedManager::SetSeed (rd());
    CommandLine cmd(__FILE__);

    cmd.AddValue("pcap", "Write PCAP traces.", pcap);
    cmd.AddValue("printRoutes", "Print routing table dumps.", printRoutes);
    cmd.AddValue("size", "Number of nodes.", size);
    cmd.AddValue("time", "Simulation time, s.", totalTime);
    cmd.AddValue("step", "Grid step, m", step);

    cmd.Parse(argc, argv);
    return true;
}

// Run 메소드 수정: FlowMonitor 설치 추가

void
AodvExample::Run()
{
    // 기존 코드 유지
    // Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (1));

    CreateNodes();
    CreateDevices();
    InstallInternetStack();
    InstallApplications();
    InstallFlowMonitor(); // FlowMonitor 설치

    std::cout << "Starting simulation for " << totalTime << " s ...\n";

    Simulator::Stop(Seconds(totalTime));
    Simulator::Run();

    // FlowMonitor 데이터 수집 및 출력
    flowMonitor->CheckForLostPackets();
    flowMonitor->SerializeToXmlFile("aodv-flowmon.xml", true, true);

    Simulator::Destroy();
}

// Report 메소드 수정: FlowMonitor 통계 출력

void
AodvExample::Report(std::ostream& os)
{
    // FlowMonitor 통계 로드
    flowMonitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    os << "FlowID\tSource\tDestination\tTxPackets\tRxPackets\tThroughput (bps)\tAverage E2E Delay (s)\n";
    for (auto const& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        double throughtput = (flow.second.rxBytes * 8.0) / totalTime; // bps 계산
        double avgDelay = 0.0;
        os << flow.first << "\t" 
           << t.sourceAddress << "\t" 
           << t.destinationAddress << "\t"
           << flow.second.txPackets << "\t" 
           << flow.second.rxPackets << "\t"
           << throughtput << "\t"
           << avgDelay << "\n";
    }
}

// CreateNodes 메소드 유지

void
AodvExample::CreateNodes()
{
    std::cout << "Creating " << (unsigned)size << " nodes " << step << " m apart.\n";
    nodes.Create(size);
    // 노드 이름 지정
    for (uint32_t i = 0; i < size; ++i)
    {
        std::ostringstream os;
        os << "node-" << i;
        Names::Add(os.str(), nodes.Get(i));
    }
    // 정적 그리드 생성
    MobilityHelper mobility;
    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
    x->SetAttribute("Min", DoubleValue(0.0));
    x->SetAttribute("Max", DoubleValue(200.0));
    Ptr<UniformRandomVariable> y = CreateObject<UniformRandomVariable>();
    y->SetAttribute("Min", DoubleValue(0.0));
    y->SetAttribute("Max", DoubleValue(200.0));

    Ptr<RandomBoxPositionAllocator> positionAlloc = CreateObject<RandomBoxPositionAllocator>();
    positionAlloc->SetX(x);
    positionAlloc->SetY(y);

    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::UniformRandomVariable[Min=20.0|Max=20.0]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"),
                            "PositionAllocator", PointerValue(positionAlloc));
    mobility.Install(nodes);
}

// CreateDevices 메소드 유지

void
AodvExample::CreateDevices()
{
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6Mbps"),
                                 "RtsCtsThreshold", UintegerValue(0));
    devices = wifi.Install(wifiPhy, wifiMac, nodes);

    if (pcap)
    {
        wifiPhy.EnablePcapAll(std::string("aodv"));
    }
}

// InstallInternetStack 메소드 유지

void
AodvExample::InstallInternetStack()
{
    AodvHelper aodv;
    // AODV 속성 설정 가능
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv); // 다음 Install()에 영향을 미칩니다.
    stack.Install(nodes);
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.0.0.0");
    interfaces = address.Assign(devices);

    if (printRoutes)
    {
        Ptr<OutputStreamWrapper> routingStream =
            Create<OutputStreamWrapper>("aodv.routes", std::ios::out);
        Ipv4RoutingHelper::PrintRoutingTableAllAt(Seconds(8), routingStream);
    }
}

// InstallApplications 메소드 유지

void
AodvExample::InstallApplications()
{
    PingHelper ping(interfaces.GetAddress(size - 1));
    ping.SetAttribute("VerboseMode", EnumValue(Ping::VerboseMode::VERBOSE));

    ApplicationContainer p = ping.Install(nodes.Get(0));
    p.Start(Seconds(0));
    p.Stop(Seconds(totalTime) - Seconds(0.001));

    // 노드 이동
    Ptr<Node> node = nodes.Get(size / 2);
    Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
    Simulator::Schedule(Seconds(totalTime / 3),
                        &MobilityModel::SetPosition,
                        mob,
                        Vector(1e5, 1e5, 1e5));
}

// FlowMonitor 설치 헬퍼 메소드 추가

void
AodvExample::InstallFlowMonitor()
{
    flowMonitor = flowHelper.InstallAll();
}

/* Modification */
// Taken straight from HPCC repo.
/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007,2008,2009 INRIA, UDCAST
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Amine Ismail <amine.ismail@sophia.inria.fr>
 *                      <amine.ismail@udcast.com>
 *
 */

#ifndef RDMA_CLIENT_H
#define RDMA_CLIENT_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include <ns3/rdma.h>
#include "ns3/random-variable-stream.h"

namespace ns3 {

class Socket;
class Packet;

/**
 * \ingroup rdmaclientserver
 * \class rdmaClient
 * \brief A RDMA client.
 *
 */
class RdmaClient : public Application
{
public:
  static TypeId
  GetTypeId (void);

  RdmaClient ();

  virtual ~RdmaClient ();

  /**
   * \brief set the remote address and port
   * \param ip remote IP address
   * \param port remote port
   */
  void SetRemote (Ipv4Address ip, uint16_t port);
  void SetLocal (Ipv4Address ip, uint16_t port);
  void SetPG (uint16_t pg);
  void SetSize(uint64_t size);
  void Finish();

protected:
  virtual void DoDispose (void);

private:

  virtual void StartApplication (void);
  virtual void StopApplication (void);

  uint64_t m_size;
  uint16_t m_pg;

  Ipv4Address m_sip, m_dip;
  uint16_t m_sport, m_dport;
  uint32_t m_win; // bound of on-the-fly packets
  uint64_t m_baseRtt; // base Rtt
public:
  void SetWorkload (uint32_t avgSize,
                    uint32_t avgTimeInterval,
                    std::map<double,int> msgSizeCDF,
                    double avgMsgSizePkts);
private:
  void Start (Time start);
  void ScheduleNextMessage ();
  void SendMessage ();
  uint32_t GetNextMsgSizeFromDist ();

  Time stopTime;
  EventId           m_nextSendEvent; //!< Event id of pending "send msg" event
  Ptr<ExponentialRandomVariable>  m_interMsgTime; //!< rng for rate of message generation in sec/msg
  std::map<double,int> m_msgSizeCDF; //!< The CDF of msg sizes {cum. prob. -> msg size in pkts}
  uint32_t m_avgSize;
  uint32_t m_avgTimeInterval;
  double m_avgMsgSizePkts; //!< Average msg size of size CDF, in pkts
  Ptr<UniformRandomVariable>      m_msgSizePkts;  //!< rng to choose msg size from the set workload
  uint16_t          m_totMsgCnt;     //!< Total number of messages sent so far
  uint32_t m_totalSizeBytes; //!< Total size in Bytes, both sent and unsent.
  uint16_t          m_maxMsgs;       //!< Maximum number of messages allowed to be sent
};

} // namespace ns3

#endif /* RDMA_CLIENT_H */

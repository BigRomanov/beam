#pragma once

#include "node_processor.h"
#include "p2p/p2p.h"
#include "../utility/io/asyncevent.h"
#include "../core/proto.h"
#include "../core/block_crypt.h"
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/set.hpp>
#include <condition_variable>

namespace beam
{
struct Node : public P2PNotifications
{
	static const uint16_t s_PortDefault = 31744; // whatever

	struct Config
	{
		io::Address m_Listen;
		std::vector<io::Address> m_Connect;

		std::string m_sPathLocal;
        ECC::NoLeak<ECC::uintBig> m_WalletKey;
		NodeProcessor::Horizon m_Horizon;

		struct Timeout {
			uint32_t m_Reconnect_ms	= 1000;
			uint32_t m_Insane_ms	= 1000 * 3600; // 1 hour
			uint32_t m_GetState_ms	= 1000 * 5;
			uint32_t m_GetBlock_ms	= 1000 * 30;
			uint32_t m_GetTx_ms		= 1000 * 5;
			uint32_t m_MiningSoftRestart_ms = 100;
		} m_Timeout;

		uint32_t m_MaxPoolTransactions = 100 * 1000;
		uint32_t m_MiningThreads = 0; // by default disabled
		uint32_t m_MinerID = 0; // used as a seed for miner nonce generation

		// Number of verification threads for CPU-hungry cryptography. Currently used for block validation only.
		// 0: single threaded
		// negative: number of cores minus number of mining threads.
		int m_VerificationThreads = 0;

		struct HistoryCompression
		{
			std::string m_sPathOutput;
			std::string m_sPathTmp;

			Height m_Threshold = 60 * 24;		// 1 day roughly. Newer blocks should not be aggregated (not mature enough)
			Height m_MinAggregate = 60 * 24;	// how many new blocks should produce new file
			uint32_t m_Naggling = 32;			// combine up to 32 blocks in memory, before involving file system
			uint32_t m_MaxBacklog = 7;
		} m_HistoryCompression;

		struct TestMode {
			// for testing only!
			uint32_t m_FakePowSolveTime_ms = 15 * 1000;

		} m_TestMode;

		std::vector<Block::Body> m_vTreasury;

        Height hImport=0;

	} m_Cfg; // must not be changed after initialization

	~Node();

    void Initialize();

	NodeProcessor& get_Processor() { return m_Processor; } // for tests only!

private:
    void ImportMacroblock(Height); // throws on err
    void Reset();

    // P2PNotifications
    virtual void on_p2p_started(P2P* p2p) override;
    virtual void on_peer_connected(StreamId id) override;
    virtual void on_peer_state_updated(StreamId id, const PeerState& newState) override;
    virtual void on_peer_disconnected(StreamId id) override;
    virtual void on_p2p_stopped() override;

	ECC::Hash::Value m_hvCfg;

	struct Processor
		:public NodeProcessor
	{
		// NodeProcessor
		virtual void RequestData(const Block::SystemState::ID&, bool bBlock, const PeerID* pPreferredPeer) override;
		virtual void OnPeerInsane(const PeerID&) override;
		virtual void OnNewState() override;
		virtual void OnRolledBack() override;
		virtual bool VerifyBlock(const Block::BodyBase&, TxBase::IReader&&, const HeightRange&) override;

		struct Verifier
		{
			const TxBase* m_pTx;
			TxBase::IReader* m_pR;
			TxBase::Context m_Context;

			bool m_bFail;
			uint32_t m_iTask;
			uint32_t m_Remaining;

			std::mutex m_Mutex;
			std::condition_variable m_Cond;

			std::vector<std::thread> m_vThreads;

			void Thread(uint32_t);

			IMPLEMENT_GET_PARENT_OBJ(Processor, m_Verifier)
		} m_Verifier;

		IMPLEMENT_GET_PARENT_OBJ(Node, m_Processor)
	} m_Processor;

	NodeProcessor::TxPool m_TxPool;

	struct State {
		enum Enum {
			Idle,
			Connecting,
			Connected,
			Snoozed,
		};
	};

	struct Peer;

	struct Task
		:public boost::intrusive::set_base_hook<>
		,public boost::intrusive::list_base_hook<>
	{
		typedef std::pair<Block::SystemState::ID, bool> Key;
		Key m_Key;

		bool m_bRelevant;
		Peer* m_pOwner;

		bool operator < (const Task& t) const { return (m_Key < t.m_Key); }
	};

	typedef boost::intrusive::list<Task> TaskList;
	typedef boost::intrusive::multiset<Task> TaskSet;

	TaskList m_lstTasksUnassigned;
	TaskSet m_setTasks;

	void TryAssignTask(Task&, const NodeDB::PeerID*);
	bool ShouldAssignTask(Task&, Peer&);
	void AssignTask(Task&, Peer&);
	void DeleteUnassignedTask(Task&);

	static uint32_t GetTime_ms();

	struct WantedTx
	{
		struct Node
			:public boost::intrusive::set_base_hook<>
			,public boost::intrusive::list_base_hook<>
		{
			Transaction::KeyType m_Key;
			uint32_t m_Advertised_ms;

			bool operator < (const Node& n) const { return (m_Key < n.m_Key); }
		};

		typedef boost::intrusive::list<Node> List;
		typedef boost::intrusive::multiset<Node> Set;

		List m_lst;
		Set m_set;

		void Delete(Node&);

		io::Timer::Ptr m_pTimer;
		void SetTimer();
		void OnTimer();

		IMPLEMENT_GET_PARENT_OBJ(beam::Node, m_Wtx)
	} m_Wtx;

	struct Peer
		: public boost::intrusive::list_base_hook<>
	{
		typedef std::unique_ptr<Peer> Ptr;

		Node* m_pThis;

		int m_iPeer; // negative if accepted connection
		void get_ID(NodeProcessor::PeerID&);

		State::Enum m_eState;

        P2P* m_p2p;
        StreamId m_streamId;
        SerializedMsg m_SerializeCache;

		Height m_TipHeight;
		proto::Config m_Config;

		TaskList m_lstTasks;
		void TakeTasks();
		void ReleaseTasks();
		void ReleaseTask(Task&);
		void SetTimerWrtFirstTask();

		io::Timer::Ptr m_pTimer;
		void OnTimer();
		void SetTimer(uint32_t timeout_ms);
		void KillTimer();

        void OnClosed(int errorCode) { }

		void OnPostError();
		static void ThrowUnexpected();

		Task& get_FirstTask();
		void OnFirstTaskDone();
		void OnFirstTaskDone(NodeProcessor::DataStatus::Enum);

		std::set<Task::Key> m_setRejected; // data that shouldn't be requested from this peer. Reset after reconnection or on receiving NewTip

    #define THE_MACRO(code, msg) \
		void Send(const proto::msg& v); \
		void OnMsg(proto::msg&& v);
		BeamNodeMsgsAll(THE_MACRO)
    #undef THE_MACRO

	};

    P2P* m_p2p=0;


	typedef boost::intrusive::list<Peer> PeerList;
	PeerList m_lstPeers;

	Peer* AllocPeer();
	void DeletePeer(Peer*);
	Peer* FindPeer(const Processor::PeerID&);

	void RefreshCongestions();

	struct PerThread
	{
		io::Reactor::Ptr m_pReactor;
		io::AsyncEvent::Ptr m_pEvt;
		std::thread m_Thread;
	};

	struct Miner
	{
		std::vector<PerThread> m_vThreads;
		io::AsyncEvent::Ptr m_pEvtMined;

		struct Task
		{
			typedef std::shared_ptr<Task> Ptr;

			// Task is mutable. But modifications are allowed only when holding the mutex.

			Block::SystemState::Full m_Hdr;
			ByteBuffer m_Body;
			Amount m_Fees;

			std::shared_ptr<volatile bool> m_pStop;
		};

		void OnRefresh(uint32_t iIdx);
		void OnMined();

		void HardAbortSafe();
		bool Restart();

		std::mutex m_Mutex;
		Task::Ptr m_pTask; // currently being-mined

		io::Timer::Ptr m_pTimer;
		bool m_bTimerPending;
		void OnTimer();
		void SetTimer(uint32_t timeout_ms, bool bHard);

		IMPLEMENT_GET_PARENT_OBJ(Node, m_Miner)
	} m_Miner;

	struct Compressor
	{
		void Init();
		void OnRolledBack();
		void Cleanup();
		void Delete(const NodeDB::StateID&);
		void OnNewState();
		void FmtPath(Block::BodyBase::RW&, Height, const Height* pH0);
		void StopCurrent();

		void OnNotify();
		void Proceed();
		bool ProceedInternal();
		bool SquashOnce(std::vector<HeightRange>&);
		bool SquashOnce(Block::BodyBase::RW&, Block::BodyBase::RW& rwSrc0, Block::BodyBase::RW& rwSrc1);

		PerThread m_Link;
		std::mutex m_Mutex;
		std::condition_variable m_Cond;

		volatile bool m_bStop;
		bool m_bEnabled;
		bool m_bSuccess;

		// current data exchanged
		HeightRange m_hrNew; // requested range. If min is non-zero - should be merged with previously-generated
		HeightRange m_hrInplaceRequest;

		IMPLEMENT_GET_PARENT_OBJ(Node, m_Compressor)
	} m_Compressor;
};

} // namespace beam

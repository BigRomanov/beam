#pragma once

#include "../core/common.h"
#include "../core/storage.h"
#include "../sqlite/sqlite3.h"

namespace beam {

class NodeDB
{
public:

	struct StateFlags {
		static const uint32_t Functional	= 0x1;	// has block body
		static const uint32_t Reachable		= 0x2;	// has only functional nodes up to the genesis state
		static const uint32_t Active		= 0x4;	// part of the current blockchain
	};

	struct ParamID {
		enum Enum {
			DbVer,
			CursorRow,
			CursorHeight,
			StateExtra,
			FossilHeight,
			SubsidyLo,
			SubsidyHi,
			SubsidyOpen,
		};
	};

	struct Query
	{
		enum Enum
		{
			Begin,
			Commit,
			Rollback,
			Scheme,
			ParamGet,
			ParamIns,
			ParamUpd,
			StateIns,
			StateDel,
			StateGet,
			StateGetHeightAndPrev,
			StateFind,
			StateFind2,
			StateUpdPrevRow,
			StateGetNextFCount,
			StateSetNextCount,
			StateSetNextCountF,
			StateGetHeightAndAux,
			StateGetNextFunctional,
			StateSetFlags,
			StateGetFlags0,
			StateGetFlags1,
			StateGetNextCount,
			StateSetPeer,
			StateGetPeer,
			TipAdd,
			TipDel,
			TipReachableAdd,
			TipReachableDel,
			EnumTips,
			EnumFunctionalTips,
			EnumAtHeight,
			EnumAncestors,
			StateGetPrev,
			Unactivate,
			Activate,
			MmrGet,
			MmrSet,
			SpendableAdd,
			SpendableDel,
			SpendableModify,
			SpendableEnum,
			SpendableEnumWithSig,
			StateGetBlock,
			StateSetBlock,
			StateDelBlock,
			StateSetRollback,
			MinedIns,
			MinedUpd,
			MinedDel,
			MinedSel,
			MacroblockEnum,
			MacroblockIns,
			MacroblockDel,

			Dbg0,
			Dbg1,
			Dbg2,
			Dbg3,
			Dbg4,

			count
		};
	};


	NodeDB();
	~NodeDB();

	void Close();
	void Open(const char* szPath);

	struct Blob {
		const void* p;
		uint32_t n;

		Blob() {}
		Blob(const void* p_, uint32_t n_) :p(p_) ,n(n_) {}
		Blob(const ByteBuffer& bb)
		{
			if ((n = (uint32_t) bb.size()))
				p = &bb.at(0);
		}
	};

	class Recordset
	{
		sqlite3_stmt* m_pStmt;
	public:

		NodeDB & m_DB;

		Recordset(NodeDB&);
		Recordset(NodeDB&, Query::Enum, const char*);
		~Recordset();

		void Reset();
		void Reset(Query::Enum, const char*);

		// Perform the query step. SELECT only: returns true while there're rows to read
		bool Step();
		void StepStrict(); // must return at least 1 row, applicable for SELECT

		// in/out
		void put(int col, uint32_t);
		void put(int col, uint64_t);
		void put(int col, const Blob&);
		void put(int col, const char*);
		void get(int col, uint32_t&);
		void get(int col, uint64_t&);
		void get(int col, Blob&);
		void get(int col, ByteBuffer&); // don't over-use

		const void* get_BlobStrict(int col, uint32_t n);

		template <typename T> void put_As(int col, const T& x) { put(col, Blob(&x, sizeof(x))); }
		template <typename T> void get_As(int col, T& out) { out = get_As<T>(col); }
		template <typename T> const T& get_As(int col) { return *(const T*) get_BlobStrict(col, sizeof(T)); }

		void putNull(int col);
		bool IsNull(int col);

		void put(int col, const Merkle::Hash& x) { put_As(col, x); }
		void get(int col, Merkle::Hash& x) { get_As(col, x); }
		void put(int col, const Block::PoW& x) { put_As(col, x); }
		void get(int col, Block::PoW& x) { get_As(col, x); }
	};

	int get_RowsChanged() const;
	uint64_t get_LastInsertRowID() const;

	class Transaction {
		NodeDB* m_pDB;
	public:
		Transaction(NodeDB* = NULL);
		Transaction(NodeDB& db) :Transaction(&db) {}
		~Transaction(); // by default - rolls back

		void Start(NodeDB&);
		void Commit();
		void Rollback();
	};

	// Hi-level functions

	void ParamSet(uint32_t ID, const uint64_t*, const Blob*);
	bool ParamGet(uint32_t ID, uint64_t*, Blob*);

	uint64_t ParamIntGetDef(int ID, uint64_t def = 0);

	uint64_t InsertState(const Block::SystemState::Full&); // Fails if state already exists

	uint64_t StateFindSafe(const Block::SystemState::ID&);
	void get_State(uint64_t rowid, Block::SystemState::Full&);

	bool DeleteState(uint64_t rowid, uint64_t& rowPrev); // State must exist. Returns false if there are ancestors.

	uint32_t GetStateNextCount(uint64_t rowid);
	uint32_t GetStateFlags(uint64_t rowid);
	void SetFlags(uint64_t rowid, uint32_t);

	void SetStateFunctional(uint64_t rowid);
	void SetStateNotFunctional(uint64_t rowid);

	typedef Merkle::Hash PeerID;

	void set_Peer(uint64_t rowid, const PeerID*);
	bool get_Peer(uint64_t rowid, PeerID&);

	void SetStateBlock(uint64_t rowid, const Blob& body);
	void GetStateBlock(uint64_t rowid, ByteBuffer& body, ByteBuffer& rollback);
	void SetStateRollback(uint64_t rowid, const Blob& rollback);
	void DelStateBlock(uint64_t rowid);

	struct StateID {
		uint64_t m_Row;
		Height m_Height;
		void SetNull();
	};


	struct WalkerState {
		Recordset m_Rs;
		StateID m_Sid;

		WalkerState(NodeDB& db) :m_Rs(db) {}
		bool MoveNext();
	};

	void EnumTips(WalkerState&); // lowest to highest
	void EnumFunctionalTips(WalkerState&); // highest to lowest
	void EnumStatesAt(WalkerState&, Height);
	void EnumAncestors(WalkerState&, const StateID&);
	bool get_Prev(StateID&);
	bool get_Prev(uint64_t&);

	bool get_Cursor(StateID& sid);

    void get_Proof(Merkle::Proof&, const StateID& sid, Height hPrev);
    void get_PredictedStatesHash(Merkle::Hash&, const StateID& sid); // For the next block.

	// the following functions move the curos, and mark the states with 'Active' flag
	void MoveBack(StateID&);
	void MoveFwd(const StateID&);

	// Utxos & kernels
	struct WalkerSpendable
	{
		const bool m_bWithSignature;

		Recordset m_Rs;
		Blob m_Key;
		Blob m_Signature;
		uint32_t m_nUnspentCount;

		WalkerSpendable(NodeDB& db, bool bWithSignature)
			:m_bWithSignature(bWithSignature)
			,m_Rs(db)
		{
		}
		bool MoveNext();
	};

	void EnumUnpsent(WalkerSpendable&);

	void AddSpendable(const Blob& key, const Blob& body, uint32_t nRefs, uint32_t nUnspentCount);
	void ModifySpendable(const Blob& key, int32_t nRefsDelta, int32_t nUnspentDelta); // will delete iff refs=0

	void assert_valid(); // diagnostic, for tests only

	void SetMined(const StateID&, const Amount&);
	bool DeleteMinedSafe(const StateID&);

	struct WalkerMined {
		Recordset m_Rs;
		StateID m_Sid;
		Amount m_Amount;

		WalkerMined(NodeDB& db) :m_Rs(db) {}
		bool MoveNext();
	};

	void EnumMined(WalkerMined&, Height hMin); // from low to high

	void EnumMacroblocks(WalkerState&); // highest to lowest
	void MacroblockIns(uint64_t rowid);
	void MacroblockDel(uint64_t rowid);

private:

	sqlite3* m_pDb;
	sqlite3_stmt* m_pPrep[Query::count];

	void TestRet(int);
	void ThrowSqliteError(int);
	static void ThrowError(const char*);
	static void ThrowInconsistent();

	void Create();
	void ExecQuick(const char*);
	bool ExecStep(sqlite3_stmt*);
	bool ExecStep(Query::Enum, const char*); // returns true while there's a row

	sqlite3_stmt* get_Statement(Query::Enum, const char*);


	void TipAdd(uint64_t rowid, Height);
	void TipDel(uint64_t rowid, Height);
	void TipReachableAdd(uint64_t rowid, Height);
	void TipReachableDel(uint64_t rowid, Height);
	void SetNextCount(uint64_t rowid, uint32_t);
	void SetNextCountFunctional(uint64_t rowid, uint32_t);
	void OnStateReachable(uint64_t rowid, uint64_t rowPrev, Height, bool);
	void BuildMmr(uint64_t rowid, uint64_t rowPrev, Height);
	void put_Cursor(const StateID& sid); // jump
	void ModifySpendableSafe(const Blob& key, int32_t nRefsDelta, int32_t nUnspentDelta);

	void TestChanged1Row();

	struct Dmmr;
};



} // namespace beam

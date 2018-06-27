#include "wallet/wallet_db.h"
#include <assert.h>
#include "test_helpers.h"

#include "utility/logger.h"
#include <boost/filesystem.hpp>

using namespace std;
using namespace ECC;
using namespace beam;

WALLET_TEST_INIT

namespace
{
    IKeyChain::Ptr createSqliteKeychain()
    {
        const char* dbName = "wallet.db";
        if (boost::filesystem::exists(dbName))
        {
            boost::filesystem::remove(dbName);
        }
        ECC::NoLeak<ECC::uintBig> seed;
        seed.V = ECC::Zero;
        auto keychain = Keychain::init(dbName, "pass123", seed);
        beam::Block::SystemState::ID id = { };
        id.m_Height = 134;
        keychain->setSystemStateID(id);
        return keychain;
    }
}

void TestKeychain()
{
	auto keychain = createSqliteKeychain();

	Coin coin1(5, Coin::Unspent, 0, 10);
	keychain->store(coin1);

    WALLET_CHECK(coin1.m_id == 1);

	Coin coin2(2, Coin::Unspent, 0, 10);
	keychain->store(coin2);

    WALLET_CHECK(coin2.m_id == 2);
    
    {
	    auto coins = keychain->getCoins(7);
        WALLET_CHECK(coins.size() == 2);

	
		vector<Coin> localCoins;
		localCoins.push_back(coin2);
		localCoins.push_back(coin1);

		for (int i = 0; i < coins.size(); ++i)
		{
            WALLET_CHECK(localCoins[i].m_id == coins[i].m_id);
            WALLET_CHECK(localCoins[i].m_amount == coins[i].m_amount);
            WALLET_CHECK(coins[i].m_status == Coin::Locked);
		}
	}

	{
		vector<Coin> coins;
		coin2.m_status = Coin::Spent;
		coins.push_back(coin2);

		keychain->update(coins);

        WALLET_CHECK(keychain->getCoins(5).size() == 0);
	}

	{
		Block::SystemState::ID a;
		Hash::Processor() << static_cast<uint32_t>(rand()) >> a.m_Hash;
		a.m_Height = rand();

		const char* name = "SystemStateID";
		keychain->setVar(name, "dummy");
		keychain->setVar(name, a);

		Block::SystemState::ID b;
        WALLET_CHECK(keychain->getVar(name, b));

		WALLET_CHECK(a == b);
	}
}

void TestStoreCoins()
{
    auto keychain = createSqliteKeychain();

  
    Coin coin = { 5, Coin::Unspent, 1, 10, KeyType::Coinbase };
    keychain->store(coin);
    coin = { 4, Coin::Unspent, 1, 10, KeyType::Comission };
    keychain->store(coin);
    coin = { 2, Coin::Unspent, 1, 10, KeyType::Regular };
    keychain->store(coin);
    coin = { 5, Coin::Unspent, 1, 10, KeyType::Coinbase };
    keychain->store(coin);
    coin = { 1, Coin::Unspent, 1, 10, KeyType::Regular };
    keychain->store(coin);
    coin = { 5, Coin::Unspent, 1, 10, KeyType::Coinbase };
    keychain->store(coin);
    coin = { 4, Coin::Unspent, 1, 10, KeyType::Comission };
    keychain->store(coin);
    coin = { 1, Coin::Unspent, 1, 10, KeyType::Regular };
    keychain->store(coin);
    coin = { 4, Coin::Unspent, 1, 10, KeyType::Comission };
    keychain->store(coin);
    coin = { 1, Coin::Unspent, 1, 10, KeyType::Regular };
    keychain->store(coin);
    coin = { 1, Coin::Unspent, 1, 10, KeyType::Regular };
    keychain->store(coin);

    auto coins = vector<Coin>{
            Coin{ 5, Coin::Unspent, 1, 10, KeyType::Coinbase },
            Coin{ 4, Coin::Unspent, 1, 10, KeyType::Comission },
            Coin{ 2, Coin::Unspent, 1, 10, KeyType::Regular },
            Coin{ 5, Coin::Unspent, 3, 10, KeyType::Coinbase },
            Coin{ 1, Coin::Unspent, 1, 10, KeyType::Regular },
            Coin{ 5, Coin::Unspent, 1, 10, KeyType::Coinbase },
            Coin{ 4, Coin::Unspent, 1, 10, KeyType::Comission },
            Coin{ 1, Coin::Unspent, 1, 10, KeyType::Regular },
            Coin{ 4, Coin::Unspent, 3, 10, KeyType::Comission },
            Coin{ 1, Coin::Unspent, 1, 10, KeyType::Regular },
            Coin{ 1, Coin::Unspent, 1, 10, KeyType::Regular } };
    keychain->store(coins);


    int coinBase = 0;
    int comission = 0;
    int regular = 0;
    keychain->visit([&coinBase, &comission, &regular](const Coin& coin)->bool
    {
        if (coin.m_key_type == KeyType::Coinbase)
        {
            ++coinBase;
        }
        else if (coin.m_key_type == KeyType::Comission)
        {
            ++comission;
        }
        else if (coin.m_key_type == KeyType::Regular)
        {
            ++regular;
        }
        return true;
    });

    WALLET_CHECK(coinBase == 2);
    WALLET_CHECK(comission == 2);
    WALLET_CHECK(regular == 10);
}
using namespace beam;
using namespace beam::wallet;
void TestStoreTxRecord()
{
    auto keychain = createSqliteKeychain();
    Uuid id = {1, 3, 4, 5 ,65};
    TxDescription tr;
    tr.m_txId = id;
    tr.m_amount = 34;
    tr.m_peerId = 23;
    tr.m_createTime = 123456;
    tr.m_sender = true;
    tr.m_status = TxDescription::InProgress;
    WALLET_CHECK_NO_THROW(keychain->saveTx(tr));
    WALLET_CHECK_NO_THROW(keychain->saveTx(tr));
    TxDescription tr2 = tr;
    tr2.m_txId = id;
    tr2.m_amount = 43;
    tr2.m_createTime = 1234564;
    tr2.m_modifyTime = 12345644;
    tr2.m_status = TxDescription::Completed;
    WALLET_CHECK_NO_THROW(keychain->saveTx(tr2));
    
    auto t = keychain->getTxHistory();
    WALLET_CHECK(t.size() == 1);
    WALLET_CHECK(t[0].m_txId == tr.m_txId);
    WALLET_CHECK(t[0].m_amount == tr.m_amount);
    WALLET_CHECK(t[0].m_peerId == tr.m_peerId);
    WALLET_CHECK(t[0].m_createTime == tr.m_createTime);
    WALLET_CHECK(t[0].m_modifyTime == tr2.m_modifyTime);
    WALLET_CHECK(t[0].m_sender == tr2.m_sender);
    WALLET_CHECK(t[0].m_status == tr2.m_status);
    Uuid id2 = { 3,4,5 };
    WALLET_CHECK_NO_THROW(keychain->deleteTx(id2));
    WALLET_CHECK_NO_THROW(keychain->deleteTx(id));

    WALLET_CHECK_NO_THROW(keychain->saveTx(tr2));
    WALLET_CHECK_NO_THROW(keychain->saveTx(tr2));
    boost::optional<TxDescription> tr3;
    WALLET_CHECK_NO_THROW(tr3 = keychain->getTx(tr2.m_txId));
    WALLET_CHECK(tr3.is_initialized());
    WALLET_CHECK(tr3->m_txId == tr2.m_txId);
    WALLET_CHECK(tr3->m_amount == tr2.m_amount);
    WALLET_CHECK(tr3->m_peerId == tr2.m_peerId);
    WALLET_CHECK(tr3->m_message == tr2.m_message);
    WALLET_CHECK(tr3->m_createTime == tr2.m_createTime);
    WALLET_CHECK(tr3->m_modifyTime == tr2.m_modifyTime);
    WALLET_CHECK(tr3->m_sender == tr2.m_sender);
    WALLET_CHECK(tr3->m_status == tr2.m_status);
    WALLET_CHECK(tr3->m_fsmState == tr2.m_fsmState);
    WALLET_CHECK_NO_THROW(keychain->deleteTx(tr2.m_txId));
    WALLET_CHECK(keychain->getTxHistory().empty());

    for (uint8_t i = 0; i < 100; ++i)
    {
        tr.m_txId[0] = i;
        WALLET_CHECK_NO_THROW(keychain->saveTx(tr));
    }
    WALLET_CHECK(keychain->getTxHistory().size() == 100);
    t = keychain->getTxHistory(50, 2);
    WALLET_CHECK(t.size() == 2);
    t = keychain->getTxHistory(99, 10);
    WALLET_CHECK(t.size() == 1);
    t = keychain->getTxHistory(9, 0);
    WALLET_CHECK(t.size() == 0);
    t = keychain->getTxHistory(50, 2);
    id[0] = 50;
    WALLET_CHECK(t[0].m_txId == id);
    id[0] = 51;
    WALLET_CHECK(t[1].m_txId == id);

    t = keychain->getTxHistory(0, 1);
    WALLET_CHECK(t.size() == 1 && t[0].m_txId[0] == 0);

    t = keychain->getTxHistory(99, 1);
    WALLET_CHECK(t.size() == 1 && t[0].m_txId[0] == 99);

    t = keychain->getTxHistory(100, 1);
    WALLET_CHECK(t.size() == 0);
}

int main() 
{
    int logLevel = LOG_LEVEL_DEBUG;
#if LOG_VERBOSE_ENABLED
    logLevel = LOG_LEVEL_VERBOSE;
#endif
    auto logger = beam::Logger::create(logLevel, logLevel);
	ECC::InitializeContext();
	TestKeychain();
    TestStoreCoins();
    TestStoreTxRecord();

    return WALLET_CHECK_RESULT;
}

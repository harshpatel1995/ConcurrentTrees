
#include <thread>
#include <mutex>

class HolderMutex : public std::recursive_mutex
{
public:
    void lock()
    {
        std::recursive_mutex::lock();
        m_holder = std::this_thread::get_id();
        m_num_locks++;
    }

    bool try_lock()
    {
        bool ret = std::recursive_mutex::try_lock();

        if (ret)
        {
            m_holder = std::this_thread::get_id();
            m_num_locks++;
        }

        return ret;
    }

    /**
    * Resets the holder id and calls unlock the specified number of times to the recursive_mutex. */
    void unlock()
    {
        m_holder = std::thread::id();

        auto temp = m_num_locks;
        m_num_locks = 0;
        while (temp > 0)
        {
            temp--;
            std::recursive_mutex::unlock();
        }
    }

    /**
    * @return true iff the recursive_mutex is locked by the caller of this method. */
    bool owns_lock() const
    {
        return m_holder == std::this_thread::get_id();
    }

private:
    std::thread::id m_holder;
    int m_num_locks = 0;
};

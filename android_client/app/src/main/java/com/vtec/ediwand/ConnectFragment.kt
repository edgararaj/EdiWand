package com.vtec.ediwand

import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.result.ActivityResultLauncher
import androidx.fragment.app.Fragment
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.lifecycleScope
import com.vtec.ediwand.databinding.FragmentConnectBinding
import kotlinx.coroutines.*
import java.util.*

class ConnectFragment : Fragment() {

    private var _binding: FragmentConnectBinding? = null

    private lateinit var removeStaleJob: Job
    private val availComputers: MutableLiveData<MutableList<ControlComputer>> = MutableLiveData( mutableListOf() )
    private lateinit var broadcastListener: ListenForBroadcastTask
    private lateinit var connectActivityLauncher: ActivityResultLauncher<Unit>

    companion object {
        lateinit var client: TcpClient
        lateinit var clientPingTimer: Timer
    }

    // This property is only valid between onCreateView and
    // onDestroyView.
    private val binding get() = _binding!!

    override fun onCreateView( inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle? ): View {
        _binding = FragmentConnectBinding.inflate(inflater, container, false)

        connectActivityLauncher = registerForActivityResult(ControlActivity.Contract()) {
            clientPingTimer.cancel()
            client.stopClient()
        }

        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        availComputers.observe(viewLifecycleOwner) {
            bindToListView(it)
        }

        removeStaleJob = lifecycleScope.launch {
            while (isActive) {
                delay(5000)
                removeStaleComputers()
            }
        }

        binding.list.addItemDecoration(ItemDecoration(1))

        /*
        binding.buttonSecond.setOnClickListener {
            findNavController().navigate(R.id.action_SecondFragment_to_FirstFragment)
        }
         */
    }

    private fun removeStaleComputers() {
        //Log.d("---------", "removing statel")
        val newComputers: MutableList<ControlComputer> = ArrayList()
        for (cc in availComputers.value!!) {
            if ((Date().time - cc.recognized.time) / 1000 < 5) newComputers.add(cc)
        }
        availComputers.postValue(newComputers)
    }

    private fun bindToListView(cc: List<ControlComputer>) {
        binding.list.adapter = context?.let { ControlComputerAdapter(cc, connectActivityLauncher) }
        if (cc.isNotEmpty()) {
            binding.list.visibility = View.VISIBLE
            binding.connectInfo.visibility = View.INVISIBLE
        } else {
            binding.list.visibility = View.INVISIBLE
            binding.connectInfo.visibility = View.VISIBLE
        }
    }

    override fun onResume() {
        super.onResume()

        broadcastListener = ListenForBroadcastTask(availComputers, broadcastPort)
        broadcastListener.run()
    }

    override fun onPause() {
        super.onPause()

        runBlocking {
            broadcastListener.job.cancelAndJoin()
        }
    }

    override fun onDestroyView() {
        Log.d("-----------", "destroy")
        super.onDestroyView()
        _binding = null
    }
}
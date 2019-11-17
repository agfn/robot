package com.lt.btrc;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

import java.io.IOException;
import java.io.OutputStream;
import java.util.Set;
import java.util.UUID;


public class MainActivity extends AppCompatActivity {

    private BluetoothAdapter bta;
    private final static String MY_UUID = "00001101-0000-1000-8000-00805F9B34FB";
    private final static byte DO_FORWARD = (byte)0xC0,
        DO_BACK = (byte)0xC1, DO_STOP = (byte)0xC2,
        DO_LEFT = (byte)0xC3, DO_RIGHT = (byte)0xC4;
    private BluetoothSocket btsock = null;
    private Button leftbtn, rightbtn, forwardbtn, backbtn;
    private OutputStream btos = null;


    private void initBT() {
        bta = BluetoothAdapter.getDefaultAdapter();
        if (bta == null) {
            AlertDialog.Builder builder = new AlertDialog.Builder(this);
            builder.setMessage("Your Devices Don't Support Bluetooth!");
            builder.setTitle("ERROR");
            AlertDialog dialog = builder.create();
            dialog.show();
        }

        if (!bta.isEnabled()) {
            Intent enableBT = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
            startActivity(enableBT);
        }

        Set<BluetoothDevice> devices = bta.getBondedDevices();
        if (devices.size() <= 0) {
            AlertDialog.Builder builder = new AlertDialog.Builder(this);
            builder.setMessage("Your must pair first!");
            builder.setTitle("WARNING");
            builder.setCancelable(false);
            AlertDialog dialog = builder.create();
            dialog.show();
        } else {
            for (BluetoothDevice dev : devices) {
                BluetoothSocket tmp = null;
                try {
                    // Get a BluetoothSocket to connect with the given BluetoothDevice.
                    // MY_UUID is the app's UUID string, also used in the server code.
                    tmp = dev.createRfcommSocketToServiceRecord(UUID.fromString(MY_UUID));
                } catch (IOException e) {
                    Log.e("", "Socket's create() method failed", e);
                }
                btsock = tmp;
                try {
                    btsock.connect();
                    btos = btsock.getOutputStream();
                } catch (Exception e){
                    Log.e("CONNECT", "fail");
                }
                Toast.makeText(this, "Connected", Toast.LENGTH_LONG).show();
            }
        }
    }

    void send(byte op){
        if (btsock == null) {
            Toast.makeText(this, "Connect First", Toast.LENGTH_LONG).show();
            return;
        }

        if (btos != null) {
            byte[] buf = new byte[1];
            buf[0] = op;
            try {
                btos.write(buf);
                btos.flush();
            } catch (IOException e) {
                Log.e("WRITE", "ERROR");
            }
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        leftbtn = (Button)findViewById(R.id.leftbtn);
        rightbtn = (Button)findViewById(R.id.rightbtn);
        forwardbtn = (Button)findViewById(R.id.forwardbtn);
        backbtn = (Button)findViewById(R.id.backbtn);

        leftbtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (btsock != null) send(DO_LEFT);
            }
        });

        rightbtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (btsock != null) send(DO_RIGHT);
            }
        });

        forwardbtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (btsock != null) send(DO_FORWARD);
            }
        });

        backbtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (btsock != null) send(DO_BACK);
            }
        });

        //
        initBT();
    }
}

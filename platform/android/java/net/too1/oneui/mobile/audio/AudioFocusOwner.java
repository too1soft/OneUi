package net.too1.oneui.mobile.audio;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import java.util.function.Consumer;

/** Foreground media focus and noisy-route policy, independent of any codec or network. */
public final class AudioFocusOwner implements AutoCloseable {
    private final Context context;
    private final AudioManager manager;
    private final Consumer<String> listener;
    private final Handler main=new Handler(Looper.getMainLooper());
    private AudioFocusRequest request;
    private boolean registered;
    private int previousVolumeStream;
    private String state="none";
    private boolean volumeBound;
    private long generation;
    private final BroadcastReceiver noisy=new BroadcastReceiver(){
        @Override public void onReceive(Context c,Intent i){
            if(AudioManager.ACTION_AUDIO_BECOMING_NOISY.equals(i.getAction()))change("route_lost");
        }
    };
    public AudioFocusOwner(Context context,Consumer<String> listener){
        this.context=context;this.listener=listener;
        manager=context.getSystemService(AudioManager.class);
    }
    public static AudioAttributes attributes(){
        return new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA)
            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build();
    }
    public String state(){return state;}
    public boolean systemMuted(){return manager==null||manager.isStreamMute(AudioManager.STREAM_MUSIC)||manager.getStreamVolume(AudioManager.STREAM_MUSIC)==0;}
    public String acquire(){
        if(request!=null)return state;
        if(manager==null){change("denied");return state;}
        final long token=++generation;
        request=new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
            .setAudioAttributes(attributes()).setAcceptsDelayedFocusGain(false).setWillPauseWhenDucked(true)
            .setOnAudioFocusChangeListener(value->{
                if(token!=generation||request==null)return;
                if(value==AudioManager.AUDIOFOCUS_GAIN)change("granted");
                else if(value==AudioManager.AUDIOFOCUS_LOSS)change("lost");
                else change("transient");
            },main).build();
        try {
            if((context.getApplicationInfo().flags&2)!=0){
                android.app.ActivityManager.RunningAppProcessInfo process=new android.app.ActivityManager.RunningAppProcessInfo();
                android.app.ActivityManager.getMyMemoryState(process);
                android.util.Log.d("OneUiAudio","Focus request importance="+process.importance+" window="+(context instanceof Activity&&((Activity)context).hasWindowFocus()));
            }
            int result=manager.requestAudioFocus(request);
            if(result==AudioManager.AUDIOFOCUS_REQUEST_GRANTED){
                if(context instanceof Activity){
                    Activity activity=(Activity)context;previousVolumeStream=activity.getVolumeControlStream();
                    activity.setVolumeControlStream(AudioManager.STREAM_MUSIC);
                    volumeBound=true;
                }
                IntentFilter filter=new IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY);
                if(Build.VERSION.SDK_INT>=33)context.registerReceiver(noisy,filter,Context.RECEIVER_NOT_EXPORTED);
                else context.registerReceiver(noisy,filter);
                registered=true;change("granted");
            }else change("denied");
        }catch(RuntimeException failure){
            manager.abandonAudioFocusRequest(request);
            if(volumeBound&&context instanceof Activity){((Activity)context).setVolumeControlStream(previousVolumeStream);volumeBound=false;}
            change("denied");
        }
        return state;
    }
    public void release(){
        ++generation;
        AudioFocusRequest old=request;request=null;
        if(old!=null&&manager!=null)manager.abandonAudioFocusRequest(old);
        if(registered){context.unregisterReceiver(noisy);registered=false;}
        if(volumeBound&&context instanceof Activity){((Activity)context).setVolumeControlStream(previousVolumeStream);volumeBound=false;}
        change("none");
    }
    private void change(String value){if(!state.equals(value)){state=value;listener.accept(value);}}
    @Override public void close(){release();}
}

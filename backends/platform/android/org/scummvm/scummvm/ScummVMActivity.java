package org.scummvm.scummvm;

import android.Manifest;
import android.annotation.TargetApi;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Rect;
import android.media.AudioManager;
import android.net.Uri;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.text.ClipboardManager;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.TypedValue;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.inputmethod.InputMethodManager;
import android.widget.ImageView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.Reader;
import java.io.UnsupportedEncodingException;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;
import java.util.Properties;

//import android.os.Environment;
//import java.util.List;

public class ScummVMActivity extends Activity implements OnKeyboardVisibilityListener {

	/* Establish whether the hover events are available */
	private static boolean _hoverAvailable;

	private ClipboardManager _clipboard;
	private File _configScummvmFile;
	private File _actualScummVMDataDir;
	private File _defaultScummVMSavesDir;


	/**
	* Id to identify an external storage read request.
	*/
	private static final int MY_PERMISSIONS_REQUEST_READ_EXT_STORAGE = 100; // is an app-defined int constant. The callback method gets the result of the request.

	static {
		try {
			MouseHelper.checkHoverAvailable(); // this throws exception if we're on too old version
			_hoverAvailable = true;
		} catch (Throwable t) {
			_hoverAvailable = false;
		}
	}

	public View.OnClickListener keyboardBtnOnClickListener = new View.OnClickListener() {
		@Override
		public void onClick(View v) {
			runOnUiThread(new Runnable() {
					public void run() {
						toggleKeyboard();
					}
				});
		}
	};


	private class MyScummVM extends ScummVM {
		public MyScummVM(SurfaceHolder holder) {
			super(ScummVMActivity.this.getAssets(), holder);
		}

		@Override
		protected void getDPI(float[] values) {
			DisplayMetrics metrics = new DisplayMetrics();
			getWindowManager().getDefaultDisplay().getMetrics(metrics);

			values[0] = metrics.xdpi;
			values[1] = metrics.ydpi;
		}

		@Override
		protected void displayMessageOnOSD(final String msg) {
			if (msg != null) {
				runOnUiThread(new Runnable() {
					public void run() {
						Toast.makeText(ScummVMActivity.this, msg, Toast.LENGTH_SHORT).show();
					}
				});
			}
		}

		@Override
		protected void openUrl(String url) {
			startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url)));
		}

		@Override
		protected boolean hasTextInClipboard() {
			return _clipboard.hasText();
		}

		@Override
		protected String getTextFromClipboard() {
			CharSequence text = _clipboard.getText();
			if (text != null) {
				return text.toString();
			}
			return null;
		}

		@Override
		protected boolean setTextInClipboard(String text) {
			_clipboard.setText(text);
			return true;
		}

		@Override
		protected boolean isConnectionLimited() {
			// The WIFI Service must be looked up on the Application Context or memory will leak on devices < Android N (According to Android Studio warning)
			WifiManager wifiMgr = (WifiManager)getApplicationContext().getSystemService(Context.WIFI_SERVICE);
			if (wifiMgr != null && wifiMgr.isWifiEnabled()) {
				WifiInfo wifiInfo = wifiMgr.getConnectionInfo();
				return (wifiInfo == null || wifiInfo.getNetworkId() == -1); //WiFi is on, but it's not connected to any network
			}
			return true;
		}

		@Override
		protected void setWindowCaption(final String caption) {
			runOnUiThread(new Runnable() {
					public void run() {
						setTitle(caption);
					}
				});
		}

		@Override
		protected void showVirtualKeyboard(final boolean enable) {
			runOnUiThread(new Runnable() {
					public void run() {
						showKeyboard(enable);
					}
				});
		}

		@Override
		protected void showKeyboardControl(final boolean enable) {
			runOnUiThread(new Runnable() {
					public void run() {
						showKeyboardView(enable);
					}
				});
		}

		@Override
		protected String[] getSysArchives() {
			return new String[0];
		}

		@Override
		protected byte[] convertEncoding(String to, String from, byte[] string) throws UnsupportedEncodingException {
			String str = new String(string, from);
			return str.getBytes(to);
		}

		@Override
		protected String[] getAllStorageLocations() {
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
			    && checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED
			) {
				requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE}, MY_PERMISSIONS_REQUEST_READ_EXT_STORAGE);
			} else {
				return ExternalStorage.getAllStorageLocations().toArray(new String[0]);
			}
			return new String[0]; // an array of zero length
		}

	}

	private MyScummVM _scummvm;
	private ScummVMEvents _events;
	private MouseHelper _mouseHelper;
	private Thread _scummvm_thread;

	@RequiresApi(api = Build.VERSION_CODES.KITKAT)
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		hideSystemUI();

		setVolumeControlStream(AudioManager.STREAM_MUSIC);

		setContentView(R.layout.main);
		takeKeyEvents(true);

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
			&& checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED
		) {
			requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE}, MY_PERMISSIONS_REQUEST_READ_EXT_STORAGE);
		}

		// REMOVED: Dialogue prompt with only option to Quit the app if !Environment.getExternalStorageDirectory().canRead()

		SurfaceView main_surface = findViewById(R.id.main_surface);

		main_surface.requestFocus();

		// REMOVED: Since getFilesDir() is guaranteed to exist, getFilesDir().mkdirs() might be related to crashes in Android version 9+ (Pie or above, API 28+)!

		// REMOVED: Setting savePath to Environment.getExternalStorageDirectory() + "/ScummVM/Saves/"
		//                            so that it will be in persistent external storage and not deleted on uninstall
		//                            This has the issue for external storage being unavailable on some devices
		//                            Is this persistence really important considering that Android does not really support it anymore
		//                               Exceptions (storage locations that are persisted or methods to access files across apps) are:
		//                                 - shareable media files (images, audio, video)
		//                                 - files stored with Storage Access Framework (SAF) which requires user interaction with FilePicker)
		//          Original fallback was getDir()
		//                            so app's internal space (which would be deleted on uninstall) was set as WORLD_READABLE which is no longer supported in newer versions of Android API
		//                            In newer APIs we can set that path as Context.MODE_PRIVATE which is the default - but this makes the files inaccessible to other apps

		//
		// seekAndInitScummvmConfiguration() returns false if something went wrong
		// when initializing configuration (or when seeking and trying to use an existing ini file) for ScummVM
		if (!seekAndInitScummvmConfiguration()) {
			Log.e(ScummVM.LOG_TAG, "Error while trying to find and/or initialize scummvm configuration file!");
			// TODO error prompt (popup to user)
		}

		_clipboard = (ClipboardManager)getSystemService(CLIPBOARD_SERVICE);

		// Start ScummVM
		_scummvm = new MyScummVM(main_surface.getHolder());

		_scummvm.setArgs(new String[] {
		    "ScummVM",
		    "--config=" + _configScummvmFile.getPath(),
		    "--path=" + _actualScummVMDataDir.getPath(),
		    "--savepath=" + _defaultScummVMSavesDir.getPath()
		});

		Log.d(ScummVM.LOG_TAG, "Hover available: " + _hoverAvailable);
		if (_hoverAvailable) {
			_mouseHelper = new MouseHelper(_scummvm);
			_mouseHelper.attach(main_surface);
		}

		if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB_MR1)
		{
			_events = new ScummVMEvents(this, _scummvm, _mouseHelper);
		}
		else
		{
			_events = new ScummVMEventsHoneycomb(this, _scummvm, _mouseHelper);
		}

		// On screen button listener
		findViewById(R.id.show_keyboard).setOnClickListener(keyboardBtnOnClickListener);

		// Keyboard visibility listener
		setKeyboardVisibilityListener(this);

		main_surface.setOnKeyListener(_events);
		main_surface.setOnTouchListener(_events);

		_scummvm_thread = new Thread(_scummvm, "ScummVM");
		_scummvm_thread.start();
	}

	@Override
	public void onStart() {
		Log.d(ScummVM.LOG_TAG, "onStart");

		super.onStart();
	}

	@Override
	public void onResume() {
		Log.d(ScummVM.LOG_TAG, "onResume");

		super.onResume();

		if (_scummvm != null)
			_scummvm.setPause(false);
		showMouseCursor(false);
	}

	@Override
	public void onPause() {
		Log.d(ScummVM.LOG_TAG, "onPause");

		super.onPause();

		if (_scummvm != null)
			_scummvm.setPause(true);
		showMouseCursor(true);
	}

	@Override
	public void onStop() {
		Log.d(ScummVM.LOG_TAG, "onStop");

		super.onStop();
	}

	@Override
	public void onDestroy() {
		Log.d(ScummVM.LOG_TAG, "onDestroy");

		super.onDestroy();

		if (_events != null) {
			_events.sendQuitEvent();

			try {
				// 1s timeout
				_scummvm_thread.join(1000);
			} catch (InterruptedException e) {
				Log.i(ScummVM.LOG_TAG, "Error while joining ScummVM thread", e);
			}

			_scummvm = null;
		}
	}


	@Override
	public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
		if (requestCode == MY_PERMISSIONS_REQUEST_READ_EXT_STORAGE) {
			// If request is cancelled, the result arrays are empty.
			if (grantResults.length > 0
				&& grantResults[0] == PackageManager.PERMISSION_GRANTED) {
				// permission was granted
				Log.i(ScummVM.LOG_TAG, "Read External Storage permission was granted at Runtime");
			} else {
				// permission denied! We won't be able to make use of functionality depending on this permission.
				Toast.makeText(this, "Until permission is granted, some storage locations may be inaccessible!", Toast.LENGTH_SHORT)
					.show();
			}
		}
	}


	@Override
	public boolean onTrackballEvent(MotionEvent e) {
		if (_events != null)
			return _events.onTrackballEvent(e);

		return false;
	}

	@Override
	public boolean onGenericMotionEvent(final MotionEvent e) {
		if (_events != null)
			return _events.onGenericMotionEvent(e);

		return false;
	}


	@Override
	public void onWindowFocusChanged(boolean hasFocus) {
	    super.onWindowFocusChanged(hasFocus);
	    if (hasFocus) {
		hideSystemUI();
	    }
	}


	// TODO setSystemUiVisibility is introduced in API 11 and deprecated in API 30 - When we move to API 30 we will have to replace this code
	//	https://developer.android.com/training/system-ui/immersive.html#java
	//
	//  The code sample in the url below contains code to switch between immersive and default mode
	//	https://github.com/android/user-interface-samples/tree/master/AdvancedImmersiveMode
	//  We could do something similar by making it a Global UI option.
	@TargetApi(Build.VERSION_CODES.KITKAT)
	private void hideSystemUI() {
		// Enables regular immersive mode.
		// For "lean back" mode, remove SYSTEM_UI_FLAG_IMMERSIVE.
		// Or for "sticky immersive," replace it with SYSTEM_UI_FLAG_IMMERSIVE_STICKY
		View decorView = getWindow().getDecorView();
		decorView.setSystemUiVisibility(
		View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
		// Set the content to appear under the system bars so that the
		// content doesn't resize when the system bars hide and show.
		| View.SYSTEM_UI_FLAG_LAYOUT_STABLE
		| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
		| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
		// Hide the nav bar and status bar
		| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
		| View.SYSTEM_UI_FLAG_FULLSCREEN);
	}

//	// Shows the system bars by removing all the flags
//	// except for the ones that make the content appear under the system bars.
//	@TargetApi(Build.VERSION_CODES.JELLY_BEAN)
//	private void showSystemUI() {
//		View decorView = getWindow().getDecorView();
//		decorView.setSystemUiVisibility(
//		    View.SYSTEM_UI_FLAG_LAYOUT_STABLE
//		    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
//		    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
//	}

	// Show or hide the Android keyboard.
	// Called by the override of showVirtualKeyboard()
	@TargetApi(Build.VERSION_CODES.CUPCAKE)
	private void showKeyboard(boolean show) {
		SurfaceView main_surface = findViewById(R.id.main_surface);
		InputMethodManager imm = (InputMethodManager)
			getSystemService(INPUT_METHOD_SERVICE);

		if (show) {
			imm.showSoftInput(main_surface, InputMethodManager.SHOW_IMPLICIT);
		} else  {
			imm.hideSoftInputFromWindow(main_surface.getWindowToken(), InputMethodManager.HIDE_IMPLICIT_ONLY);
		}
	}

	// Toggle showing or hiding the virtual keyboard.
	// Called by keyboardBtnOnClickListener()
	@TargetApi(Build.VERSION_CODES.CUPCAKE)
	private void toggleKeyboard() {
		SurfaceView main_surface = findViewById(R.id.main_surface);
		InputMethodManager imm = (InputMethodManager)
			getSystemService(INPUT_METHOD_SERVICE);

		imm.toggleSoftInputFromWindow(main_surface.getWindowToken(),
		                              InputMethodManager.SHOW_IMPLICIT,
		                              InputMethodManager.HIDE_IMPLICIT_ONLY);
	}

	// Show or hide the semi-transparent keyboard btn (which is used to explicitly bring up the android keyboard).
	// Called by the override of showKeyboardControl()
	private void showKeyboardView(boolean show) {
		ImageView keyboardBtn = findViewById(R.id.show_keyboard);

		if (show) {
			keyboardBtn.setVisibility(View.VISIBLE);
		} else {
			keyboardBtn.setVisibility(View.GONE);
		}
	}

	private void showMouseCursor(boolean show) {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
			SurfaceView main_surface = findViewById(R.id.main_surface);
			int type = show ? PointerIcon.TYPE_DEFAULT : PointerIcon.TYPE_NULL;
			main_surface.setPointerIcon(PointerIcon.getSystemIcon(this, type));
		} else {
			/* Currently hiding the system mouse cursor is only
			   supported on OUYA.  If other systems provide similar
			   intents, please add them here as well */
			Intent intent =
				new Intent(show?
					   "tv.ouya.controller.action.SHOW_CURSOR" :
					   "tv.ouya.controller.action.HIDE_CURSOR");
			sendBroadcast(intent);
		}
	}

	// Listener to check for keyboard visibility changes
	// https://stackoverflow.com/a/36259261
	private void setKeyboardVisibilityListener(final OnKeyboardVisibilityListener onKeyboardVisibilityListener) {
		final View parentView = ((ViewGroup) findViewById(android.R.id.content)).getChildAt(0);
		parentView.getViewTreeObserver().addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {

			private boolean alreadyOpen;
			private final int defaultKeyboardHeightDP = 100;
			private final int EstimatedKeyboardDP = defaultKeyboardHeightDP + (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP ? 48 : 0);
			private final Rect rect = new Rect();

			@TargetApi(Build.VERSION_CODES.CUPCAKE)
			@Override
			public void onGlobalLayout() {
				    int estimatedKeyboardHeight = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, EstimatedKeyboardDP, parentView.getResources().getDisplayMetrics());
				    parentView.getWindowVisibleDisplayFrame(rect);
				    int heightDiff = parentView.getRootView().getHeight() - (rect.bottom - rect.top);
				    boolean isShown = heightDiff >= estimatedKeyboardHeight;

				    if (isShown == alreadyOpen) {
					Log.i("Keyboard state", "Ignoring global layout change...");
					return;
				    }
				    alreadyOpen = isShown;
				    onKeyboardVisibilityListener.onVisibilityChanged(isShown);
			}
		});
	}

	@Override
	public void onVisibilityChanged(boolean visible) {
//		Toast.makeText(HomeActivity.this, visible ? "Keyboard is active" : "Keyboard is Inactive", Toast.LENGTH_SHORT).show();
		hideSystemUI();
	}

	// Auxiliary function to overwrite a file (used for overwriting the scummvm.ini file with an existing other one)
	@RequiresApi(api = Build.VERSION_CODES.KITKAT)
	private static void copyFileUsingStream(File source, File dest) throws IOException {
		try (InputStream is = new FileInputStream(source); OutputStream os = new FileOutputStream(dest)) {
			byte[] buffer = new byte[1024];
			int length;
			while ((length = is.read(buffer)) > 0) {
				os.write(buffer, 0, length);
			}
		}
	}

	/**
	 * Auxiliary function to read our ini configuration file
	 * Code is from https://stackoverflow.com/a/41084504
	 * returns The sections of the ini file as a Map of the header Strings to a Properties object (the key=value list of each section)
	 */
	@TargetApi(Build.VERSION_CODES.GINGERBREAD)
	private static Map<String, Properties> parseINI(Reader reader) throws IOException {
		final HashMap<String, Properties> result = new HashMap<>();
		new Properties() {

			private Properties section;

			@Override
			public Object put(Object key, Object value) {
				String header = (key + " " + value).trim();
				if (header.startsWith("[") && header.endsWith("]"))
					return result.put(header.substring(1, header.length() - 1),
						section = new Properties());
				else
					return section.put(key, value);
			}

		}.load(reader);
		return result;
	}

	@RequiresApi(api = Build.VERSION_CODES.KITKAT)
	private static String getVersionInfoFromScummvmConfiguration(String fullIniFilePath) {
		try (BufferedReader bufferedReader = new BufferedReader(new FileReader(fullIniFilePath))) {
			Map<String, Properties> parsedIniMap = parseINI(bufferedReader);
			if (!parsedIniMap.isEmpty() && parsedIniMap.containsKey("scummvm")) {
				return Objects.requireNonNull(parsedIniMap.get("scummvm")).getProperty("versioninfo", "");
			}
		} catch (IOException ignored) {
		} catch (NullPointerException ignored) {
		}
		return "";
	}

	@RequiresApi(api = Build.VERSION_CODES.KITKAT)
	private boolean seekAndInitScummvmConfiguration() {

		_actualScummVMDataDir = getExternalFilesDir(null);
		if (_actualScummVMDataDir == null || !_actualScummVMDataDir.canRead()) {
			new AlertDialog.Builder(this)
				.setTitle(R.string.no_external_files_dir_access_title)
				.setIcon(android.R.drawable.ic_dialog_alert)
				.setMessage(R.string.no_external_files_dir_access)
				.setNegativeButton(R.string.quit,
					new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface dialog, int which) {
							finish();
						}
					})
				.show();
			return false;
		}

		Log.d(ScummVM.LOG_TAG, "Base ScummVM data folder is: " + _actualScummVMDataDir.getPath());

		File[] extfiles = _actualScummVMDataDir.listFiles();
		if (extfiles != null) {
			Log.d(ScummVM.LOG_TAG, "Size: "+ extfiles.length);
			for (File extfile : extfiles) {
				Log.d(ScummVM.LOG_TAG, "FileName:" + extfile.getName());
			}
		}

		File externalScummVMConfigDir = new File(_actualScummVMDataDir, ".config/scummvm");
		if (!externalScummVMConfigDir.exists() && externalScummVMConfigDir.mkdirs()) {
			Log.d(ScummVM.LOG_TAG, "Created ScummVM Config path: " + externalScummVMConfigDir.getPath());
		} else if (externalScummVMConfigDir.isDirectory()) {
			Log.d(ScummVM.LOG_TAG, "ScummVM Config path already exists: " + externalScummVMConfigDir.getPath());
		} else {
			Log.e(ScummVM.LOG_TAG, "Could not create folder for ScummVM Config path: " + externalScummVMConfigDir.getPath());
			new AlertDialog.Builder(this)
				.setTitle(R.string.no_config_file_title)
				.setIcon(android.R.drawable.ic_dialog_alert)
				.setMessage(R.string.no_config_file)
				.setNegativeButton(R.string.quit,
					new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface dialog, int which) {
							finish();
						}
					})
				.show();
		}

		// TODO search in other known places for scummvm.ini too
		_configScummvmFile = new File(_actualScummVMDataDir, "scummvm.ini");
		try {
			if (_configScummvmFile.exists() || !_configScummvmFile.createNewFile()) {
				Log.d(ScummVM.LOG_TAG, "ScummVM Config file already exists!");
				Log.d(ScummVM.LOG_TAG, "Existing ScummVM INI: " + _configScummvmFile.getPath());
				String existingVersionInfo = getVersionInfoFromScummvmConfiguration(_configScummvmFile.getPath());
				if (!"".equals(existingVersionInfo.trim())) {
					Log.d(ScummVM.LOG_TAG, "Existing ScummVM Version: " + existingVersionInfo.trim());
				} else {
					Log.d(ScummVM.LOG_TAG, "Could not find info on existing ScummVM version. Old or corrupt file?");
				}
			} else {
				Log.d(ScummVM.LOG_TAG, "ScummVM Config file was created!");
				Log.d(ScummVM.LOG_TAG, "New ScummVM INI: " + _configScummvmFile.getPath());
				// if there was an old scummvmrc file (old config file), then copy that over the empty new scummvm.ini
				File oldScummVMconfig = getFileStreamPath("scummvmrc");
				if (!oldScummVMconfig.exists()) {
					Log.d(ScummVM.LOG_TAG, "Old config (scummvmrc) ScummVM file was not found!");
				} else {
					Log.d(ScummVM.LOG_TAG, "Old config (scummvmrc) ScummVM file was found!");
					String existingVersionInfo = getVersionInfoFromScummvmConfiguration(_configScummvmFile.getPath());
					if (!"".equals(existingVersionInfo.trim())) {
						Log.d(ScummVM.LOG_TAG, "Existing ScummVM Version: " + existingVersionInfo.trim());
					} else {
						Log.d(ScummVM.LOG_TAG, "Could not find info on existing ScummVM version. Old or corrupt file?");
					}
					copyFileUsingStream(oldScummVMconfig, _configScummvmFile);
					Log.d(ScummVM.LOG_TAG, "Old config (scummvmrc) ScummVM file was renamed and overwrote the new (empty) scummvm.ini");
				}
			}
		} catch(Exception e) {
			e.printStackTrace();
			new AlertDialog.Builder(this)
				.setTitle(R.string.no_config_file_title)
				.setIcon(android.R.drawable.ic_dialog_alert)
				.setMessage(R.string.no_config_file)
				.setNegativeButton(R.string.quit,
					new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface dialog, int which) {
							finish();
						}
					})
				.show();
		}


		// Set global savepath
		// TODO what if the old save-game path is no longer accessible (due to missing SD card, or newer Android API/OS version more strict restrictions)
		// TODO changing the save path to this, app specific location we should consider:
		//      - moving or copying old save files from old location (if accessible)
		//      - allowing the user to set a user-defined location via Storage Access Framework, that would override this location!
		// TODO This would override/ overwrite(?) actual old save-game path stored in the config file!
		//
		// By default choose to store savegames on app's external storage, which means they're ACCESSIBLE BY OTHER APPS, BUT they will still get DELETED ON UNINSTALL!
		//
		_defaultScummVMSavesDir = new File(_actualScummVMDataDir, "saves");
		//
		// TODO what about old save paths from plain android port? do we favor them?
		if (!_defaultScummVMSavesDir.exists() && _defaultScummVMSavesDir.mkdirs()) {
			Log.d(ScummVM.LOG_TAG, "Created ScummVM saves path: " + _defaultScummVMSavesDir.getPath());
		} else if (_defaultScummVMSavesDir.isDirectory()) {
			Log.d(ScummVM.LOG_TAG, "ScummVM saves path already exists: " + _defaultScummVMSavesDir.getPath());
		} else {
			Log.e(ScummVM.LOG_TAG, "Could not create folder for ScummVM saves path: " + _defaultScummVMSavesDir.getPath());
			new AlertDialog.Builder(this)
				.setTitle(R.string.no_config_file_title)
				.setIcon(android.R.drawable.ic_dialog_alert)
				.setMessage(R.string.no_config_file)
				.setNegativeButton(R.string.quit,
					new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface dialog, int which) {
							finish();
						}
					})
				.show();
		}

		return true;
	}

}

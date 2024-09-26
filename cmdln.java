/*
  CMDLN - All Rights Reserved. Copyright (C) 2011 Lai KamLeong
  Inspired by Bayden's SlickRun [http://www.bayden.com/slickrun/]

  Re-using many sample codes from the Internet
*/

import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.util.*;

public class cmdln extends Frame implements MouseListener, MouseMotionListener {
  // Java does not directly support constants. However, a static final variable is effectively a constant.
  // The static modifier causes the variable to be available without loading an instance of the class where it is defined.
  // The final modifier causes the variable to be unchangeable.
  static final String[] JRE_VER = System.getProperty("java.version").split("\\.");
  static final String   COMSPEC = "cmd.exe /c start";

  static cmdln     hMain;
  static TextField hEdit;
  static PopupMenu hContext;
  static Point     start_drag, start_loc;

  static String    g_szModuleFileName="", g_szModuleParam="", g_szModuleCfg="";
  static String    g_szJRE = "C:\\Program Files (x86)\\Java\\jre6\\bin\\javaw.exe";
  static String    g_szWebBrowser = "C:\\Program Files\\Internet Explorer\\iexplore.exe";
  static String    g_szLastCmdln="";

  /*------------------------------------------------------------------*/

  private static boolean copyText(String txt) {
    java.awt.Toolkit.getDefaultToolkit().getSystemClipboard().setContents(
      new java.awt.datatransfer.StringSelection(txt),
      null
    );
    return true;
  }

  private static void setMyWindowPos(Container hwnd, int mypos) {
    Point location = hwnd.getLocationOnScreen(); // getLocation() ==> relative to the parent
    Dimension screenSize = java.awt.Toolkit.getDefaultToolkit().getScreenSize();
    Dimension windowSize = hwnd.getSize();
    int x=location.x, y=location.y;

    System.out.println("screenSize = " + screenSize.width + ":" + screenSize.height);
    if (mypos==0) { //top
      x = (screenSize.width-windowSize.width)/2;
      y = 0;
    } else if (mypos==225) { //lower-left
      x = 10; y = 520;
    } else if (mypos==180) { //bottom
      x = (screenSize.width-windowSize.width)/2;
      y = screenSize.height-windowSize.height-55;
    }

    hwnd.setBounds(x, y, windowSize.width, windowSize.height);
    return;
  }

  /*------------------------------------------------------------------*/

  private static String getenv(String strKey) {
    if ( Integer.parseInt(JRE_VER[0]) > 1 || Integer.parseInt(JRE_VER[1]) >= 5 ) {
      // >= 1.5
      return System.getenv(strKey);
    }
    try {
      Process p = java.lang.Runtime.getRuntime().exec(
        "cmd.exe /c echo %" + strKey + "%"
      );
      InputStreamReader inStreamR = new InputStreamReader(p.getInputStream());
      Reader reader = new BufferedReader(inStreamR);
      Writer writer = new StringWriter();
      char[] buffer = new char[1024];
      int n; while ( (n=reader.read(buffer)) != -1 ) {
        writer.write(buffer, 0, n);
      }
      String strVal = writer.toString().trim();
      inStreamR.close();  reader.close();  writer.close();
      return strVal;
    } catch (IOException e) {
      System.out.println("getenv() failed: " + e.getMessage());
      return "";
    }
  }

  // http://stackoverflow.com/questions/4752817/expand-environment-variables-in-text
  private static String expandEnvVars(String text) {
    //System.out.println("expandEnvVars="+text);
    java.util.regex.Pattern expr = java.util.regex.Pattern.compile(
      // "\\$\\{([A-Za-z0-9]+)\\}"
      "%([A-Za-z0-9]+)%"
    );
    java.util.regex.Matcher matcher = expr.matcher(text);
    while (matcher.find()) {
      String envValue = getenv(matcher.group(1).toUpperCase());
      if (envValue==null) {
        envValue = "";
      } else {
        envValue = envValue.replaceAll("\\\\", "\\\\\\\\");
      }
      //System.out.println("envValue="+envValue);
      java.util.regex.Pattern subexpr = java.util.regex.Pattern.compile(
        "\\Q"+matcher.group(0)+"\\E"
      );
      text = subexpr.matcher(text).replaceAll(envValue);
    }
    return text;
  }

  /*------------------------------------------------------------------*/

  private static char hexDigit(char ch, int offset) {
    int val = (ch >> offset) & 0xF;
    if(val <= 9) return (char) ('0' + val);
    return (char) ('A' + val - 10);
  }
 
  private static String escapifyStr(String str) {
    String result = "";
    //if (true) return result;
    int len = str.length();
    for (int x=0; x<len; x++) {
      //if (x > 100) return result;
      char ch = str.charAt(x);
      if (ch=='\\') {
        result += "\\\\";
        continue;
      } else if ( (int)ch <= 0x007E ) {
        result += ch;
        continue;
      } else {
        result += "\\u" + hexDigit(ch, 12) + hexDigit(ch, 8) + hexDigit(ch, 4) + hexDigit(ch, 0);
      }
    }
    return result;
  }

  /*------------------------------------------------------------------*/

  private static String mapShortCut(String strCmdln) {
    InputStream          inStream;
    ByteArrayInputStream inStream2;
    String               strCmdln2 = strCmdln;

    //System.out.println("[" + strCmdln2 + "]");
    try {
      inStream = new FileInputStream(g_szModuleCfg);
      InputStreamReader inStreamR = new InputStreamReader(inStream, "UTF-8");
      Writer writer = new StringWriter();
      char[] buffer = new char[1024];
      try {
        Reader reader = new BufferedReader(inStreamR);
        int n; while ( (n=reader.read(buffer)) != -1 ) {
          writer.write(buffer, 0, n);
        }
      } finally {
        inStreamR.close();  inStream.close();
      }
      String inputString = writer.toString();
      inputString = inputString.replaceAll("\\\\","\\\\\\\\");
      //inputString = escapifyStr(inputString);
      //System.out.println(inputString); if (true) return strCmdln;
      inStream2 = new ByteArrayInputStream( inputString.getBytes("ISO-8859-1") );
    } catch (Exception e) {
      System.out.println("Error opening [" + g_szModuleCfg + "]");
      System.out.println(e);
      return strCmdln;
    }

    try {
      Properties p = new Properties(); // import java.util.*;
      // The java.util.Properites class uses the "ISO 8859-1" encoding 
      // when reading contents from an input file. This is a major 
      // predicament when the input file contains non-plain Enlgish text.
      p.load(inStream2);  //p.list(System.out); //dump all
      strCmdln2 = p.getProperty(strCmdln);
      if (strCmdln2==null) return strCmdln;

      if ( strCmdln2.charAt(0)=='"' && strCmdln2.charAt(strCmdln2.length()-1)=='"' ) {
        //to remove the outer double quote [auto-removed in WinAPI]
        strCmdln2 = strCmdln2.substring(1, strCmdln2.length()-1);
      }

      if ( strCmdln2.charAt(0)=='*' || strCmdln2.charAt(0)=='-') {
        // elevation|noWebImg not supported in java
        strCmdln2 = strCmdln2.substring(1);
      }

      //System.out.println(strCmdln + " => " + strCmdln2);
    } catch (Exception e) {
      System.out.println("Error loading [" + g_szModuleCfg + "]\n" + e);
    }

    if (strCmdln2==null) return strCmdln;
    return strCmdln2;
  }

  /*------------------------------------------------------------------*/

  private static int execCmd(int r, String strCmdln) {
    int returnVal=0;

    if ( strCmdln.indexOf("$")==0 ) {
      hEdit.setText( expandEnvVars(mapShortCut(strCmdln.substring(1))) );
      return 0;
    } else if ( strCmdln.equals("//") ) {
      hEdit.setText(g_szLastCmdln);
      return 0;
    } else {
      strCmdln = mapShortCut(strCmdln);
    }
    System.out.println(strCmdln);

    if (++r > 10) {
      System.out.println("aborted: too many levels, possibly infinite loop!");
      return 1;
    }

    if ( strCmdln.indexOf("&&")==0 ) {
      String[] cmdArr = strCmdln.split("&&");
      for (int i=0; i<cmdArr.length; i++) {
        if ( cmdArr[i]==null ) continue;
        cmdArr[i] = cmdArr[i].trim();
        if ( cmdArr[i].equals("") ) continue;
        if (i > 10) {
          System.out.println("too many levels, possibly infinite loop!");
          return 1;
        }
        System.out.println("cmdArr["+i+"]="+cmdArr[i]);
        execCmd(r, cmdArr[i]);
      }
      return 0;
    }

    if ( strCmdln.equals("/quit") ) {
      java.awt.Toolkit.getDefaultToolkit().getSystemEventQueue().postEvent(
        new WindowEvent(hMain, WindowEvent.WINDOW_CLOSING)
      );
      return 0;
    } else if ( strCmdln.equals("/c0") ) {
      setMyWindowPos(hMain, 225);
      return 0;
    } else if ( strCmdln.equals("/c1") ) {
      setMyWindowPos(hMain, 180);
      return 0;
    } else if ( strCmdln.equals("/top") ) {
      setMyWindowPos(hMain, 0);
      return 0;
    } else if ( strCmdln.indexOf("/goto")==0 ) {
      String[] paramArr = strCmdln.substring(6).split("\\s");
      hMain.setBounds(
        Integer.parseInt(paramArr[0]), Integer.parseInt(paramArr[1]),
        hMain.getWidth(), hMain.getHeight()
      );
      return 0;
    } else if ( strCmdln.indexOf("/resize")==0 ) {
      Point location = hMain.getLocationOnScreen();
      String[] paramArr = strCmdln.substring(8).split("\\s");
      hMain.setBounds(location.x, location.y,
        20+Integer.parseInt(paramArr[0]), hMain.getHeight()
      );
      return 0;
    } else if ( strCmdln.indexOf("/cpytxt")==0 ) {
      copyText(strCmdln.substring(8));
      return 0;
    } else if ( strCmdln.equals("/set") ) {
      System.getProperties().list(System.out); //dump all
      return 0;
    } else if ( strCmdln.equals("/os") ) {
      hEdit.setText( System.getProperty("os.name") + " [" + getenv("OS") + "]" );
      return 0;
    } else if ( strCmdln.equals("/ver") ) {
      hEdit.setText( System.getProperty("java.version") );
      javax.swing.JOptionPane.showMessageDialog(
        hMain, "Text Message", g_szModuleFileName,
        javax.swing.JOptionPane.INFORMATION_MESSAGE
      );
      return 0;
    } else if ( strCmdln.equals("/reload") ) {
      if (
       javax.swing.JOptionPane.showConfirmDialog(
         hMain, "Quit & Restart Application?", g_szModuleFileName,
         javax.swing.JOptionPane.OK_CANCEL_OPTION, javax.swing.JOptionPane.INFORMATION_MESSAGE
       ) == javax.swing.JOptionPane.CANCEL_OPTION
      ) return 0;
      strCmdln = "\"" + g_szJRE + "\" " + g_szModuleFileName;
      try {
        Process p = java.lang.Runtime.getRuntime().exec(strCmdln);
      } catch (IOException e) {
        System.out.println("exec failed: " + e.getMessage());
      } finally {
        System.exit(0);
      }
    } else if ( strCmdln.equals("/make") ) {
      try {
        Process p = java.lang.Runtime.getRuntime().exec(COMSPEC+" makeJava.bat");
      } catch (IOException e) {
        System.out.println("exec failed: " + e.getMessage());
      } finally {
        System.exit(0);
      }
    } else if ( strCmdln.equals("%comspec%") ) {
      strCmdln = COMSPEC;
    } else if ( strCmdln.indexOf("http:")==0 || strCmdln.indexOf("about:")==0 ) {
      System.out.println("url: "+strCmdln);
      String webBrowser = mapShortCut("webBrowser");
      if (webBrowser=="webBrowser") webBrowser=g_szWebBrowser;
      strCmdln = "\"" + webBrowser +  "\" " + strCmdln;
    }

    try {
      Process p = java.lang.Runtime.getRuntime().exec(
        expandEnvVars(strCmdln)
      );
      g_szLastCmdln=strCmdln;  hEdit.setText("");
    } catch (IOException e) {
      System.out.println("exec failed: " + e.getMessage());
      javax.swing.JOptionPane.showMessageDialog(
        hMain, e.getMessage(), g_szModuleFileName,
        javax.swing.JOptionPane.INFORMATION_MESSAGE
      );
    }
    //p.getInputStream()

    return returnVal;
  }

  /*------------------------------------------------------------------*/

  cmdln() {
    ActionListener myActionListener =  new ActionListener() {
      public void actionPerformed(ActionEvent evt) {
        // THIS CODE IS EXECUTED WHEN RETURN IS TYPED
        // * TextField txtbox = (TextField)evt.getSource();
        // * hMain == ((Component)evt.getSource()).getParent()
        String strCmdln = evt.getActionCommand(); // hEdit.getText();
        execCmd( 0, strCmdln.trim() );
      }
    };

    final Label caption = new Label("#");
    caption.setFont(new Font("Courier", Font.BOLD, 10));
    caption.setAlignment(Label.CENTER);
    caption.setForeground(Color.yellow);
    caption.setBackground(Color.black);
    caption.addMouseListener(this);
    caption.addMouseMotionListener(this);

    hEdit = new TextField("", 20);
    hEdit.setFont(new Font("FixedSys", Font.BOLD, 13));
    hEdit.setBackground(new Color(200,210,220));
    hEdit.addActionListener(myActionListener);

    hContext = new PopupMenu("ContextMenu");
     MenuItem mi;
      mi = new MenuItem("top");
      mi.addActionListener(myActionListener);
     hContext.add(mi);
      mi = new MenuItem("taskmgr");
      mi.addActionListener(myActionListener);
      //mi.addActionListener( (ActionListener)mi0.getListeners(ActionListener.class)[0] );
     hContext.add(mi);
     hContext.addSeparator();
      mi = new MenuItem("cfg");
      mi.addActionListener(myActionListener);
     hContext.add(mi);
      mi = new MenuItem("exit");
      mi.addActionListener(myActionListener);
     hContext.add(mi);

    setTitle("cmdln");
    //setDefaultCloseOperation(Frame.DO_NOTHING_ON_CLOSE); // will handle it ourselves
    //setAlwaysOnTop(true); // >=1.5
    setUndecorated(true); // >=1.4
    setResizable(false); // disable maximize
    setLayout(new BorderLayout()); //FlowLayout,BorderLayout, 
    add(caption, BorderLayout.LINE_START);
    add(hEdit);
    add(hContext);
    pack();
  } // cmdln() - constructor

  /*------------------------------------------------------------------*/

  public static void main(String[] args) {
    hMain = new cmdln();
     g_szModuleFileName = hMain.getClass().getName();
     for (int i=0; i<args.length; i++) g_szModuleParam+=args[i]+" ";
     g_szModuleParam = g_szModuleParam.trim();
     g_szModuleCfg = g_szModuleFileName + ".ini";

    if ( java.awt.GraphicsEnvironment.isHeadless() ){
      // Run console mode
      System.out.println("console mode ... ");
    } else {
      // Start in GUI mode
      System.out.println("gui mode ... ");
    }

    hMain.setVisible(true);  setMyWindowPos(hMain, 0);
    if ( args.length > 0 ) execCmd(0, g_szModuleParam);

    hMain.addWindowListener(
     new WindowAdapter() {
      public void windowClosing(WindowEvent e) {
        System.out.println("windowClosing ... ");
        if (
         javax.swing.JOptionPane.showConfirmDialog(
           hMain, "Quit?", g_szModuleFileName,
           javax.swing.JOptionPane.OK_CANCEL_OPTION, javax.swing.JOptionPane.INFORMATION_MESSAGE
         ) == javax.swing.JOptionPane.CANCEL_OPTION
        ) return;
        ((Window)e.getSource()).dispose();
        //exitProcedure();
        System.exit(0);
      }
     }
    );

    hMain.addWindowStateListener(
     new java.awt.event.WindowStateListener() {
      public void windowStateChanged(java.awt.event.WindowEvent evt) {
        if ( (evt.getNewState() & Frame.ICONIFIED)!=0 ) {
          System.out.println("minimize ... ");
          ((Frame)evt.getSource()).setState(Frame.NORMAL); //auto-restore it
        }
      }
     }
    );

  } // public static void main

  /*------------------------------------------------------------------*/

  public void mouseClicked(MouseEvent e) {
    if (e.getButton()==MouseEvent.BUTTON3) {
      hContext.show(e.getComponent(), e.getX(), e.getY());
    } else if (e.getClickCount()==2) {
      execCmd(0, "taskmgr");
    }
  }
  public void mouseEntered(MouseEvent e) {
  }
  public void mouseExited(MouseEvent e) {
  }
  public void mousePressed(MouseEvent e) {
    Point cursor = e.getPoint();
    this.start_drag = cursor;
    this.start_loc = this.getLocation();
    //System.out.println("mousePressed [" + cursor.getX() + "," + cursor.getY() + "] ");
  }
  public void mouseReleased(MouseEvent e) {
    Point current = e.getPoint();
    Point offset = new Point(
      (int) current.getX() - (int) start_drag.getX(),
      (int) current.getY() - (int) start_drag.getY()
    );
    Point new_location = new Point(
      (int) (this.start_loc.getX() + offset.getX()), 
      (int) (this.start_loc.getY() + offset.getY())
    );
    this.setLocation(new_location);
  }
  public void mouseDragged(MouseEvent e) {
  }
  public void mouseMoved(MouseEvent e) {
  }

  /*------------------------------------------------------------------*/

} // public class cmdln


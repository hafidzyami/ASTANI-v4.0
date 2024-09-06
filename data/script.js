document.addEventListener("DOMContentLoaded", function () {
  // Redirect to login if not logged in
  const restrictedPages = ["home.html", "cek-tanah.html", "lapor.html"];
  if (
    restrictedPages.some((page) => window.location.pathname.endsWith(page)) &&
    !sessionStorage.getItem("loggedIn")
  ) {
    window.location.href = "/";
  }

  const loginForm = document.getElementById("loginForm");
  const logoutButton = document.getElementById("logoutButton");
  const errorElement = document.getElementById("error");

  // Simulate user database
  const users = {
    petani1: "Simalungun12",
    petani2: "Karo23",
    petani3: "Dari34",
    petani5: "Tapanuli56",
    petani6: "Toba67",
    petani7: "Medan78",
    petani8: "Humbahas89",
    petani9: "Deli90",
    petani10: "Sipiso11",
  };

  // Handle login form submission
  if (loginForm) {
    loginForm.addEventListener("submit", function (event) {
      event.preventDefault();
      const username = document.getElementById("username").value;
      const password = document.getElementById("password").value;

      if (users[username] && users[username] === password) {
        sessionStorage.setItem("loggedIn", "true");
        window.location.href = "home.html";
      } else {
        errorElement.textContent = "Nama pengguna atau kata sandi salah";
        setTimeout(function () {
          errorElement.textContent = "";
        }, 5000);
      }
    });
  }

  // Handle logout
  if (logoutButton) {
    logoutButton.addEventListener("click", function () {
      sessionStorage.removeItem("loggedIn");
      window.location.href = "/";
    });
  }
});
